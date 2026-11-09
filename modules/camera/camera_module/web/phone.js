"use strict";
const $ = id => document.getElementById(id);
const token = new URLSearchParams(location.hash.slice(1)).get("token");
let stream = null, pc = null, session = null, generation = 0, reconnectTimer = null;
let statsTimer = null, retries = 0, active = false, connecting = false, lastAdapt = 0;
const presets = {1080:{width:1920,height:1080,frameRate:30},720:{width:1280,height:720,frameRate:30},480:{width:854,height:480,frameRate:20}};
const status = text => { $("status").textContent = text; };
function controls(running) {
  $("start").disabled = running; $("stop").disabled = !running;
  $("facing").disabled = running; $("quality").disabled = running;
}
async function request(path, data) {
  const response = await fetch(path, {method:"POST", headers:{"Content-Type":"application/json","X-Pairing-Token":token || ""}, body:JSON.stringify(data), signal:AbortSignal.timeout(15000)});
  if (!response.ok) throw new Error(await response.text());
  return response.json();
}
function waitForIce(peer) {
  if (peer.iceGatheringState === "complete") return Promise.resolve();
  return new Promise((resolve, reject) => {
    const cleanup = () => { clearTimeout(timer); peer.removeEventListener("icegatheringstatechange", check); };
    const check = () => { if (peer.iceGatheringState === "complete") {cleanup(); resolve();} };
    const timer = setTimeout(() => {cleanup(); reject(new Error("连接信息收集超时，请检查网络"));}, 10000);
    peer.addEventListener("icegatheringstatechange", check);
  });
}
async function closeConnection() {
  if (pc) { pc.onconnectionstatechange = null; pc.close(); pc = null; }
  const oldSession = session; session = null;
  if (oldSession) { try { await request("/disconnect", {session:oldSession}); } catch (_) {} }
}
async function connect(expectedGeneration) {
  connecting = true;
  await closeConnection();
  if (!active || expectedGeneration !== generation) return;
  const peer = new RTCPeerConnection({iceServers:[]}); pc = peer;
  stream.getVideoTracks().forEach(track => peer.addTrack(track, stream));
  peer.onconnectionstatechange = () => {
    if (pc !== peer || !active) return;
    if (peer.connectionState === "connected") {
      retries = 0; connecting = false; $("badge").textContent = "已连接";
      clearTimeout(reconnectTimer); status("正在向电脑传输画面，请保持此页面在前台。");
    } else if (["failed", "disconnected"].includes(peer.connectionState)) {
      connecting = false; scheduleReconnect();
    } else if (peer.connectionState === "closed") {
      connecting = false; scheduleReconnect();
    }
  };
  try {
    await peer.setLocalDescription(await peer.createOffer()); await waitForIce(peer);
    if (!active || expectedGeneration !== generation) return;
    const answer = await request("/offer", {sdp:peer.localDescription.sdp,type:"offer"});
    if (!active || expectedGeneration !== generation) {
      await request("/disconnect", {session:answer.session}); return;
    }
    session = answer.session;
    await peer.setRemoteDescription({type:answer.type,sdp:answer.sdp});
    clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(() => { if (pc === peer && peer.connectionState !== "connected") {connecting = false; scheduleReconnect();} }, 18000);
  } catch (error) {
    if (active && expectedGeneration === generation) { connecting = false; scheduleReconnect(error.message); }
  }
}
function scheduleReconnect(message = "视频连接中断") {
  clearTimeout(reconnectTimer);
  if (!active) return;
  if (retries >= 3) { stop(); status(`${message}。自动重连未成功，请检查电脑端服务后重新开始。`); return; }
  retries += 1; $("badge").textContent = "重连中";
  status(`${message}，正在重连（${retries}/3）…`);
  reconnectTimer = setTimeout(() => connect(generation), 1000 * 2 ** retries);
}
async function updateStats() {
  if (!stream) return;
  const settings = stream.getVideoTracks()[0]?.getSettings();
  if (settings) $("details").textContent = `实际采集 ${settings.width || "—"} × ${settings.height || "—"} · ${Math.round(settings.frameRate || 0)} 帧/秒`;
  if (!pc || pc.connectionState !== "connected" || !$("adaptive").checked || Date.now()-lastAdapt < 15000) return;
  try {
    const report = await pc.getStats(); let weak = false;
    report.forEach(s => { if (s.type === "remote-inbound-rtp" && s.kind === "video" && (s.fractionLost > .08 || s.roundTripTime > .3)) weak = true; });
    if (weak && settings.height > 480) {
      lastAdapt = Date.now();
      await stream.getVideoTracks()[0].applyConstraints({width:{ideal:854},height:{ideal:480},frameRate:{ideal:20,max:20}});
      status("检测到网络波动，已请求降低采集质量。实际效果取决于手机支持情况。");
    }
  } catch (_) { /* Browser statistics and constraint support vary. */ }
}
async function start() {
  if (active) return;
  if (!token) {status("缺少配对信息，请扫描电脑端二维码。"); return;}
  if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
    status("当前连接不能访问摄像头。请完成本地 HTTPS 证书信任，并使用系统浏览器打开。"); return;
  }
  active = true; retries = 0; const current = ++generation;
  controls(true); status("正在请求摄像头权限…");
  try {
    const preset = presets[$("quality").value];
    const acquired = await navigator.mediaDevices.getUserMedia({audio:false,video:{facingMode:{ideal:$("facing").value},width:{ideal:preset.width},height:{ideal:preset.height},frameRate:{ideal:preset.frameRate,max:preset.frameRate}}});
    if (!active || current !== generation) {acquired.getTracks().forEach(t => t.stop()); return;}
    stream = acquired; $("preview").srcObject = stream;
    stream.getVideoTracks()[0].onended = () => {stop(); status("摄像头已停止，请重新开始。");};
    $("badge").textContent = "连接中"; status("正在连接电脑…");
    statsTimer = setInterval(updateStats, 2000); updateStats();
    await connect(current);
  } catch (error) {
    if (current !== generation) return;
    stop();
    const messages = {NotAllowedError:"未获得摄像头权限，请在浏览器设置中允许后重试。",NotFoundError:"未找到可用摄像头。",NotReadableError:"摄像头可能正在被其他应用占用。"};
    status(messages[error.name] || error.message);
  }
}
function stop() {
  active = false; generation += 1; connecting = false;
  clearTimeout(reconnectTimer); clearInterval(statsTimer);
  if (stream) { stream.getTracks().forEach(t => {t.onended = null; t.stop();}); stream = null; }
  $("preview").srcObject = null; closeConnection(); controls(false);
  $("badge").textContent = "未连接"; status("已停止采集，摄像头已释放。");
}
$("start").addEventListener("click", start); $("stop").addEventListener("click", stop);
window.addEventListener("pagehide", () => { if (stream) stream.getTracks().forEach(t => t.stop()); if (pc) pc.close(); });
if (!token) status("请先扫描电脑端的配对二维码。");
