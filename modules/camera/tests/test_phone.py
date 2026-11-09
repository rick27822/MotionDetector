"""Real HTTPS/ICE/DTLS/RTP loopback, synthetic video; no camera required."""
import asyncio
from pathlib import Path
import ssl
import tempfile
import unittest

import numpy as np
from aiohttp import ClientSession
from aiortc import RTCPeerConnection, RTCSessionDescription, RTCConfiguration, VideoStreamTrack
from av import VideoFrame

from camera_module import CameraEngine, PreprocessConfig
from camera_module.phone import PhoneServer


class TestTrack(VideoStreamTrack):
    async def recv(self):
        pts, time_base = await self.next_timestamp()
        image = np.zeros((120,160,3),np.uint8)
        image[:] = (20,80,210)
        frame = VideoFrame.from_ndarray(image,format="bgr24")
        frame.pts, frame.time_base = pts,time_base
        return frame


class PhoneIntegrationTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.engine = CameraEngine(PreprocessConfig(64,64))
        self.server = PhoneServer(self.engine,Path(self.temp.name),"127.0.0.1",0)
        await asyncio.to_thread(self.server.start)
        self.base = f"https://127.0.0.1:{self.server.port}"
        self.tls = ssl.create_default_context(cafile=str(self.server.ca_path))
        self.client = ClientSession()
        self.peers = []

    async def asyncTearDown(self):
        for peer in self.peers: await peer.close()
        await self.client.close()
        await asyncio.to_thread(self.server.stop)
        self.engine.stop()
        self.temp.cleanup()

    async def post(self,path,data,token=None):
        return await self.client.post(self.base+path,json=data,ssl=self.tls,
                    headers={"X-Pairing-Token":token or self.server.token})

    async def connect(self):
        peer = RTCPeerConnection(RTCConfiguration(iceServers=[]))
        self.peers.append(peer)
        peer.addTrack(TestTrack())
        await peer.setLocalDescription(await peer.createOffer())
        response = await self.post("/offer",{"sdp":peer.localDescription.sdp,"type":"offer"})
        self.assertEqual(response.status,200,await response.text())
        answer = await response.json()
        await peer.setRemoteDescription(RTCSessionDescription(sdp=answer["sdp"],type="answer"))
        return peer,answer

    async def test_pairing_video_disconnect_and_reconnect(self):
        response = await self.client.get(self.base+"/phone",ssl=self.tls)
        self.assertEqual(response.status,200)
        self.assertIn("手机摄像头采集",await response.text())
        self.assertIn("frame-ancestors",response.headers["Content-Security-Policy"])
        response = await self.post("/offer",{},token="wrong-token")
        self.assertEqual(response.status,401)
        response = await self.post("/offer",{"type":"wrong"})
        self.assertEqual(response.status,400)
        peer,answer = await self.connect()
        packet = await asyncio.to_thread(self.engine.latest,None,8)
        self.assertIsNotNone(packet,"No decoded WebRTC frame arrived")
        self.assertEqual(packet.source,"phone")
        self.assertEqual(packet.source_size,(160,120))
        response = await self.client.get(self.base+"/status",ssl=self.tls,
                         headers={"X-Pairing-Token":self.server.token})
        self.assertEqual(response.status,200)
        self.assertEqual((await response.json())["packet"]["source"],"phone")
        # Real lossy codec, so use tolerance rather than exact RGB equality.
        np.testing.assert_allclose(packet.image[32,32], [210,80,20],atol=12)
        response = await self.post("/offer",{"sdp":peer.localDescription.sdp,"type":"offer"})
        self.assertEqual(response.status,409)
        response = await self.post("/disconnect",{"session":answer["session"]})
        self.assertEqual(response.status,200)
        self.assertIsNone(self.engine.latest())
        await peer.close()
        new_peer,new_answer = await self.connect()
        new_packet = await asyncio.to_thread(self.engine.latest,None,8)
        self.assertIsNotNone(new_packet)
        self.assertNotEqual(new_packet.session_id,packet.session_id)
        # A late disconnect from the previous session must not kill the new one.
        response = await self.post("/disconnect",{"session":answer["session"]})
        self.assertEqual(response.status,200)
        await asyncio.sleep(.1)
        self.assertEqual(self.engine.stats()["status"],"running")

    async def test_malformed_requests_leave_receiver_usable(self):
        for value in [None, [], {}, {"session": 3}, {"session": ""}]:
            response = await self.post("/disconnect", value)
            self.assertEqual(response.status, 400, await response.text())
        response = await self.post("/offer", {"sdp": "invalid", "type": "offer"})
        self.assertEqual(response.status, 400)
        self.assertIsNone(self.server._pc)
        _, answer = await self.connect()
        packet = await asyncio.to_thread(self.engine.latest, None, 8)
        self.assertIsNotNone(packet)
        response = await self.post("/disconnect", {"session": answer["session"]})
        self.assertEqual(response.status, 200)
        result = self.engine.read_frame()
        self.assertEqual(result.status, "disconnected")

    async def test_same_server_can_restart_with_fresh_pairing(self):
        await self.connect()
        self.assertIsNotNone(await asyncio.to_thread(self.engine.latest, None, 8))
        old_token = self.server.token
        await asyncio.to_thread(self.server.stop)
        self.assertFalse(self.server.alive)
        self.assertIsNone(self.engine.latest())
        await asyncio.to_thread(self.server.stop)  # Idempotent.
        await asyncio.to_thread(self.server.start)
        self.assertTrue(self.server.alive)
        self.assertNotEqual(old_token, self.server.token)
        response = await self.post("/offer", {}, token=old_token)
        self.assertEqual(response.status, 401)
        await self.connect()
        packet = await asyncio.to_thread(self.engine.latest, None, 8)
        self.assertIsNotNone(packet)


if __name__ == "__main__": unittest.main()
