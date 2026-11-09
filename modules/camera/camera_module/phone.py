"""Optional, local-network HTTPS + WebRTC receiver. No public/cloud services."""
from __future__ import annotations

import asyncio
import datetime as dt
import ipaddress
from pathlib import Path
import secrets
import socket
import ssl
import threading

from aiohttp import web
from aiortc import RTCPeerConnection, RTCSessionDescription, RTCConfiguration
from aiortc.mediastreams import MediaStreamError
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID


def local_addresses():
    addresses = {"127.0.0.1"}
    try:
        for item in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            address = item[4][0]
            if not address.startswith("169.254."):
                addresses.add(address)
    except OSError:
        pass
    return sorted(addresses, key=lambda x: (x.startswith("127."), x))


def create_certificates(directory, host):
    """Make a private local CA and a TLS leaf. The user explicitly installs trust."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    ca_key_file, ca_file = directory / "local-ca.key", directory / "local-ca.cer"
    now = dt.datetime.now(dt.timezone.utc)
    if ca_file.exists() and ca_key_file.exists():
        ca_key = serialization.load_pem_private_key(ca_key_file.read_bytes(), password=None)
        ca = x509.load_pem_x509_certificate(ca_file.read_bytes())
    else:
        ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "PoseLab Local Development CA")])
        ca = (x509.CertificateBuilder().subject_name(name).issuer_name(name)
              .public_key(ca_key.public_key()).serial_number(x509.random_serial_number())
              .not_valid_before(now-dt.timedelta(minutes=5)).not_valid_after(now+dt.timedelta(days=365))
              .add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True)
              .add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
              .sign(ca_key, hashes.SHA256()))
        ca_key_file.write_bytes(ca_key.private_bytes(serialization.Encoding.PEM,
                                serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        ca_file.write_bytes(ca.public_bytes(serialization.Encoding.PEM))
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    host_ip = ipaddress.ip_address(host)
    leaf = (x509.CertificateBuilder()
            .subject_name(x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "PoseLab Camera")]))
            .issuer_name(ca.subject).public_key(key.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now-dt.timedelta(minutes=5)).not_valid_after(now+dt.timedelta(days=30))
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.IPAddress(host_ip), x509.DNSName("localhost")]), critical=False)
            .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
            .sign(ca_key, hashes.SHA256()))
    key_file, cert_file = directory / "server.key", directory / "server.crt"
    key_file.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                         serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    cert_file.write_bytes(leaf.public_bytes(serialization.Encoding.PEM))
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(cert_file, key_file)
    return context, ca_file


class PhoneServer:
    def __init__(self, engine, runtime_dir, host, port=8443):
        ipaddress.ip_address(host)
        if not 0 <= port <= 65535:
            raise ValueError("端口无效")
        self.engine, self.runtime_dir, self.host, self.port = engine, Path(runtime_dir), host, port
        self.token = secrets.token_urlsafe(24)
        self._thread = None
        self._ready = threading.Event()
        self._stop_requested = threading.Event()
        self._error = None
        self._loop = None
        self._runner = None
        self._pc = None
        self._session = None
        self._tasks = set()
        self._pending = None
        self._frame_event = None
        self.message = "尚未启动"

    @property
    def url(self):
        host = f"[{self.host}]" if ":" in self.host else self.host
        return f"https://{host}:{self.port}/phone#token={self.token}"

    @property
    def alive(self):
        return bool(self._thread and self._thread.is_alive() and self._error is None)

    def start(self):
        if self.alive:
            return
        if self._thread and self._thread.is_alive():
            raise RuntimeError("手机服务尚未完全退出")
        self._ready.clear()
        self._stop_requested.clear()
        self._error = None
        self.token = secrets.token_urlsafe(24)
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        if not self._ready.wait(10):
            self._stop_requested.set()
            raise RuntimeError("手机服务启动超时")
        if self._error:
            raise RuntimeError(str(self._error))

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        try:
            self._loop.run_until_complete(self._start())
            self._ready.set()
            if not self._stop_requested.is_set():
                self._loop.run_forever()
        except Exception as exc:
            self._error, self.message = exc, str(exc)
            self._ready.set()
        finally:
            self._loop.run_until_complete(self._shutdown())
            pending = asyncio.all_tasks(self._loop)
            for task in pending:
                task.cancel()
            if pending:
                self._loop.run_until_complete(asyncio.gather(*pending, return_exceptions=True))
            self._loop.run_until_complete(self._loop.shutdown_asyncgens())
            self._loop.run_until_complete(self._loop.shutdown_default_executor())
            self._loop.close()

    async def _start(self):
        self._lock = asyncio.Lock()
        context, self.ca_path = create_certificates(self.runtime_dir / "certs", self.host)

        @web.middleware
        async def headers(request, handler):
            response = await handler(request)
            response.headers["Cache-Control"] = "no-store"
            response.headers["Referrer-Policy"] = "no-referrer"
            response.headers["X-Content-Type-Options"] = "nosniff"
            response.headers["Content-Security-Policy"] = "default-src 'self'; script-src 'self'; style-src 'self'; media-src 'self' blob:; connect-src 'self'; frame-ancestors 'none'"
            return response

        app = web.Application(client_max_size=128*1024, middlewares=[headers])
        app.router.add_get("/phone", self._page)
        app.router.add_get("/phone.js", self._script)
        app.router.add_get("/phone.css", self._style)
        app.router.add_post("/offer", self._offer)
        app.router.add_post("/disconnect", self._disconnect)
        app.router.add_get("/status", self._status)
        self._runner = web.AppRunner(app, access_log=None)
        await self._runner.setup()
        site = web.TCPSite(self._runner, self.host, self.port, ssl_context=context)
        await site.start()
        # Port zero is used by the automated integration test.
        self.port = site._server.sockets[0].getsockname()[1]
        self.message = "服务就绪，等待手机连接"

    async def _page(self, request):
        return web.FileResponse(Path(__file__).with_name("web") / "phone.html")

    async def _script(self, request):
        return web.FileResponse(Path(__file__).with_name("web") / "phone.js")

    async def _style(self, request):
        return web.FileResponse(Path(__file__).with_name("web") / "phone.css")

    def _authorize(self, request):
        if not secrets.compare_digest(request.headers.get("X-Pairing-Token", "").encode("utf-8"), self.token.encode("ascii")):
            raise web.HTTPUnauthorized(text="配对信息无效，请重新扫描电脑上的二维码")

    async def _status(self, request):
        self._authorize(request)
        data = self.engine.stats()
        packet = self.engine.latest()
        if packet is not None:
            data["packet"] = packet.metadata()
        return web.json_response(data)

    async def _offer(self, request):
        self._authorize(request)
        try:
            data = await request.json()
            if data.get("type") != "offer" or not isinstance(data.get("sdp"), str):
                raise ValueError()
        except (ValueError, AttributeError):
            raise web.HTTPBadRequest(text="连接请求格式无效")
        async with self._lock:
            if self._pc:
                raise web.HTTPConflict(text="已有手机连接，请先在电脑端断开")
            pc = RTCPeerConnection(RTCConfiguration(iceServers=[]))
            self._pc = pc
            try:
                self._session = await asyncio.to_thread(self.engine.start_external)
            except Exception:
                self._pc = None
                await pc.close()
                raise web.HTTPServiceUnavailable(text="旧输入源尚未释放，请稍后重试")
            session = self._session
            self._frame_event = asyncio.Event()
            has_video = False

            @pc.on("track")
            def on_track(track):
                nonlocal has_video
                if track.kind == "video" and not has_video:
                    has_video = True
                    self._spawn(self._receive(track, pc, session))
                    self._spawn(self._process(pc, session))

            @pc.on("connectionstatechange")
            async def on_state():
                if pc.connectionState == "connected":
                    self.message = "手机已连接"
                elif pc.connectionState in ("failed", "closed") and self._pc is pc:
                    await self._close_peer(pc)

            try:
                await pc.setRemoteDescription(RTCSessionDescription(sdp=data["sdp"], type="offer"))
                if not has_video:
                    raise ValueError("请求中没有视频轨道")
                await pc.setLocalDescription(await pc.createAnswer())
                return web.json_response({"sdp": pc.localDescription.sdp, "type": "answer", "session": session})
            except Exception as exc:
                await self._close_peer(pc)
                raise web.HTTPBadRequest(text=f"视频连接失败：{exc}")

    def _spawn(self, coroutine):
        task = asyncio.create_task(coroutine)
        self._tasks.add(task)
        task.add_done_callback(self._tasks.discard)

    async def _receive(self, track, pc, session):
        try:
            while self._pc is pc:
                frame = await asyncio.wait_for(track.recv(), timeout=15)
                self._pending = frame  # Capacity one: new frames replace old ones.
                self._frame_event.set()
        except (MediaStreamError, asyncio.TimeoutError):
            pass
        except asyncio.CancelledError:
            return
        except Exception as exc:
            self.message = f"手机接收中断：{exc}"
        finally:
            if self._pc is pc:
                await self._close_peer(pc)

    async def _process(self, pc, session):
        try:
            while self._pc is pc:
                await self._frame_event.wait()
                self._frame_event.clear()
                frame, self._pending = self._pending, None
                if frame is not None:
                    await asyncio.to_thread(self.engine.ingest, frame.to_ndarray(format="bgr24"), session, frame.time)
        except asyncio.CancelledError:
            return
        except Exception as exc:
            self.message = f"图像处理失败：{exc}"
            await self._close_peer(pc)

    async def _close_peer(self, pc=None):
        pc = pc or self._pc
        if pc is None or pc is not self._pc:
            return
        self._pc = None
        session, self._session = self._session, None
        self._pending = None
        tasks = [task for task in self._tasks if task is not asyncio.current_task()]
        for task in tasks:
            task.cancel()
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
        await pc.close()
        self.engine.finish_external(session)
        self.message = "手机已断开，可重新连接"

    async def _disconnect(self, request):
        self._authorize(request)
        try:
            data = await request.json()
            if not isinstance(data, dict) or not isinstance(data.get("session"), str) or not data["session"]:
                raise ValueError()
        except (ValueError, TypeError):
            raise web.HTTPBadRequest(text="断开请求须包含非空 session")
        async with self._lock:
            if data["session"] == self._session:
                await self._close_peer()
        return web.json_response({"ok": True})

    async def _disconnect_current(self):
        async with self._lock:
            await self._close_peer()

    def disconnect(self):
        if self.alive:
            return asyncio.run_coroutine_threadsafe(self._disconnect_current(), self._loop)

    async def _shutdown(self):
        await self._close_peer()
        if self._runner:
            await self._runner.cleanup()
            self._runner = None

    def stop(self):
        self._stop_requested.set()
        if self._thread and self._thread.is_alive():
            if not self._ready.wait(10):
                raise RuntimeError("手机服务仍在启动，稍后重试停止")
            if self._loop and not self._loop.is_closed():
                try:
                    self._loop.call_soon_threadsafe(self._loop.stop)
                except RuntimeError:
                    pass  # Thread finished between is_closed and the call.
            self._thread.join(timeout=10)
            if self._thread.is_alive():
                raise RuntimeError("手机服务尚未退出")
        self.message = "手机服务已停止"
