"""Launch the desktop UI. Import camera_module directly for algorithm integration."""
import argparse
from dataclasses import asdict
import json
import os
from pathlib import Path
import queue
import shutil
import sys
import threading
import time

ROOT = Path(__file__).resolve().parent
BG, PANEL, CARD, LINE = "#0b1017", "#101823", "#172231", "#28364a"
TEXT, MUTED, ACCENT, CYAN = "#edf3fa", "#91a2b8", "#ff9f1c", "#58dbcd"


def prepare_tk_paths():
    """Some Windows redirected paths require relative Tcl library paths."""
    library = Path(sys.base_prefix) / "tcl"
    local_library = ROOT / "runtime" / "tcl"
    for variable, folder in (("TCL_LIBRARY", "tcl8.6"), ("TK_LIBRARY", "tk8.6")):
        if variable not in os.environ and (library / folder).is_dir():
            if not (local_library / folder).is_dir():
                shutil.copytree(library / folder, local_library / folder)
            os.environ[variable] = os.path.relpath(local_library / folder).replace("\\", "/")


# Tcl snapshots its environment when its DLL is first loaded: configure first.
prepare_tk_paths()
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import cv2
import numpy as np
from PIL import Image, ImageTk
from camera_module import CameraEngine, CaptureConfig, PreprocessConfig, save_frame


def create_root():
    return tk.Tk()


class App:
    def __init__(self, root, demo=False):
        self.root = root
        self.root.title("PoseLab · 摄像头与图像处理")
        # Tk fonts scale with DPI; pixel-based geometry must scale as well.
        self.ui_scale = max(1.0, self.root.winfo_fpixels("1i") / 96)
        screen_w, screen_h = self.root.winfo_screenwidth(), self.root.winfo_screenheight()
        width = min(int(1240 * self.ui_scale), screen_w - 80)
        height = min(int(820 * self.ui_scale), screen_h - 100)
        self.root.geometry(f"{width}x{height}")
        self.root.minsize(min(width, int(1080 * self.ui_scale)),
                          min(height, int(760 * self.ui_scale)))
        self.root.configure(bg=BG)
        self.runtime = ROOT / "runtime"
        self.runtime.mkdir(exist_ok=True)
        self.engine = CameraEngine()
        self.phone = None
        self.events = queue.Queue()
        self.busy = False
        self.last_id = -1
        self.last_packet = None
        self._photo = None
        self._qr_photo = None
        self.source_var = tk.StringVar(value="演示画面")
        self.camera_var = tk.StringVar(value="0")
        self.file_var = tk.StringVar()
        self.capture_size = tk.StringVar(value="1280 × 720")
        self.capture_fps = tk.StringVar(value="30")
        self.out_size = tk.StringVar(value="640 × 640")
        self.rotation = tk.StringVar(value="0")
        self.color = tk.StringVar(value="RGB")
        self.mirror = tk.BooleanVar(value=False)
        self.normalize = tk.BooleanVar(value=False)
        self.padding = tk.StringVar(value="24")
        self.preview_original = tk.BooleanVar(value=False)
        self._load_settings()
        self._style()
        self._build()
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(40, self.tick)
        if demo:
            self.root.after(250, self.start_local)

    def _style(self):
        self.root.option_add("*Font", ("Microsoft YaHei UI", 10))
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TCombobox", fieldbackground=CARD, background=LINE, foreground=TEXT,
                        arrowcolor=TEXT, bordercolor=LINE, padding=7)
        style.map("TCombobox", fieldbackground=[("readonly", CARD)], foreground=[("readonly", TEXT)])
        style.configure("TCheckbutton", background=PANEL, foreground=TEXT, padding=4)
        style.map("TCheckbutton", background=[("active", CARD)])

    def text(self, parent, text="", size=10, color=TEXT, bold=False, **kwargs):
        return tk.Label(parent, text=text, bg=parent["bg"], fg=color,
                        font=("Microsoft YaHei UI", size, "bold" if bold else "normal"), **kwargs)

    def button(self, parent, text, command, primary=False, **kwargs):
        return tk.Button(parent, text=text, command=command, bg=ACCENT if primary else CARD,
                         fg="#15100a" if primary else TEXT, activebackground="#ffb347" if primary else LINE,
                         activeforeground="#15100a" if primary else TEXT, relief="flat", bd=0,
                         padx=16, pady=10, cursor="hand2", **kwargs)

    def entry(self, parent, variable, width=20):
        return tk.Entry(parent, textvariable=variable, bg=CARD, fg=TEXT, insertbackground=TEXT,
                        relief="flat", width=width, highlightthickness=1, highlightbackground=LINE)

    def combo(self, parent, variable, values, width=20):
        return ttk.Combobox(parent, textvariable=variable, values=values, state="readonly", width=width)

    def _build(self):
        sidebar = tk.Frame(self.root, bg=PANEL, width=int(220 * self.ui_scale))
        sidebar.pack(side="left", fill="y")
        sidebar.pack_propagate(False)
        self.text(sidebar, "PoseLab", 23, ACCENT, True).pack(anchor="w", padx=22, pady=(28, 3))
        self.text(sidebar, "摄像头与图像处理", 10, MUTED).pack(anchor="w", padx=24)
        self.text(sidebar, "MODULE 04.2", 9, MUTED).pack(anchor="w", padx=24, pady=(8, 28))
        self.nav = {}
        for key, title in [("live", "实时采集"), ("phone", "手机连接"), ("settings", "图像处理")]:
            button = self.button(sidebar, title, lambda k=key:self.show(k), anchor="w")
            button.pack(fill="x", padx=12, pady=5)
            self.nav[key] = button
        self.text(sidebar, "独立运行 · 本地处理", 10, MUTED).pack(side="bottom", pady=(0, 22))
        self.side_status = self.text(sidebar, "●  未开始采集", 10, CYAN, wraplength=170, justify="left")
        self.side_status.pack(side="bottom", padx=20, pady=18, anchor="w")
        main = tk.Frame(self.root, bg=BG)
        main.pack(side="left", fill="both", expand=True, padx=26, pady=24)
        self.title = self.text(main, "实时采集", 23, bold=True)
        self.title.pack(anchor="w")
        self.subtitle = self.text(main, "连接输入源，检查画面，再交给姿态识别模块。", 10, MUTED)
        self.subtitle.pack(anchor="w", pady=(7, 22))
        self.body = tk.Frame(main, bg=BG)
        self.body.pack(fill="both", expand=True)
        self.pages = {k:tk.Frame(self.body, bg=BG) for k in self.nav}
        self._build_live(self.pages["live"])
        self._build_phone(self.pages["phone"])
        self._build_settings(self.pages["settings"])
        self.footer = self.text(main, "就绪", 10, MUTED, anchor="w", wraplength=980, justify="left")
        self.footer.pack(fill="x", pady=(14, 0))
        self.show("live")

    def show(self, key):
        titles = {"live":("实时采集", "连接输入源，检查画面，再交给姿态识别模块。"),
                  "phone":("手机连接", "在同一 Wi-Fi 下，把手机画面传输到这台电脑。"),
                  "settings":("图像处理", "统一图像规格，并保留关键点映射回原图所需的信息。")}
        for name, page in self.pages.items():
            page.pack_forget()
            self.nav[name].configure(bg=CARD if name == key else PANEL, fg=ACCENT if name == key else MUTED)
        self.pages[key].pack(fill="both", expand=True)
        self.title.configure(text=titles[key][0]); self.subtitle.configure(text=titles[key][1])
        self.current_page = key

    def _build_live(self, parent):
        metrics = tk.Frame(parent, bg=BG)
        metrics.pack(fill="x", pady=(0, 18))
        self.metrics = {}
        for name, label in [("fps", "实际处理帧率"), ("source", "原始尺寸"), ("output", "输出尺寸"), ("cost", "预处理耗时")]:
            card = tk.Frame(metrics, bg=PANEL, padx=18, pady=12)
            card.pack(side="left", fill="both", expand=True, padx=(0, 8))
            self.text(card, label, 9, MUTED).pack(anchor="w")
            self.metrics[name] = self.text(card, "—", 20, CYAN if name == "fps" else TEXT, True)
            self.metrics[name].pack(anchor="w", pady=(5, 0))
        control = tk.Frame(parent, bg=BG)
        control.pack(fill="x", pady=(0, 12))
        self.combo(control, self.source_var, ["演示画面", "电脑摄像头", "视频文件"], 13).pack(side="left")
        self.text(control, "设备编号", 9, MUTED).pack(side="left", padx=(16, 6))
        self.entry(control, self.camera_var, 4).pack(side="left", ipady=8)
        self.button(control, "选择视频", self.choose_video).pack(side="left", padx=9)
        self.button(control, "开始采集", self.start_local, True).pack(side="left", padx=4)
        self.button(control, "停止", self.stop_capture).pack(side="left", padx=4)
        self.button(control, "保存当前帧", self.snapshot).pack(side="right")
        self.file_label = self.text(parent, "演示画面使用合成图案，不会打开摄像头。", 9, MUTED, anchor="w")
        self.file_label.pack(fill="x", pady=(0, 10))
        self.canvas = tk.Canvas(parent, bg="#080c12", highlightthickness=1, highlightbackground=LINE)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.bind("<Configure>", lambda event:self.draw())
        bottom = tk.Frame(parent, bg=BG)
        bottom.pack(fill="x", pady=(10, 0))
        self.preview_note = self.text(bottom, "预处理画面 · 保持宽高比并补边", 9, MUTED)
        self.preview_note.pack(side="left")
        self.frame_note = self.text(bottom, "尚无图像帧", 9, MUTED)
        self.frame_note.pack(side="right")
        ttk.Checkbutton(bottom, text="显示原图", variable=self.preview_original,
                        command=self.draw).pack(side="left", padx=16)

    def _build_phone(self, parent):
        left = tk.Frame(parent, bg=PANEL, padx=24, pady=22)
        left.pack(side="left", fill="both", expand=True, padx=(0, 16))
        self.text(left, "扫码连接手机", 17, bold=True).pack(anchor="w")
        self.qr_label = self.text(left, "启动手机服务后\n在这里显示配对二维码", 12, MUTED, width=28, height=14)
        self.qr_label.pack(pady=20)
        self.phone_url = tk.StringVar(value="尚未启动")
        self.entry(left, self.phone_url, 35).pack(fill="x", ipady=9)
        self.button(left, "复制连接地址", lambda:self.copy(self.phone_url.get())).pack(fill="x", pady=12)
        self.text(left, "首次使用需在手机上信任本地证书。\n打开项目说明，按步骤完成后扫码。", 10, MUTED, justify="left", wraplength=330).pack(anchor="w", pady=8)
        right = tk.Frame(parent, bg=BG, width=390)
        right.pack(side="right", fill="both", expand=True)
        self.text(right, "连接设置", 17, bold=True).pack(anchor="w", pady=(4, 20))
        try:
            from camera_module.phone import local_addresses
            addresses = local_addresses()
        except ImportError:
            addresses = ["127.0.0.1"]
        self.host_var = tk.StringVar(value=addresses[0])
        self.text(right, "电脑局域网 IP（手机需能访问）", 10, MUTED).pack(anchor="w")
        self.host_combo = ttk.Combobox(right, textvariable=self.host_var, values=addresses)
        self.host_combo.pack(fill="x", pady=(8, 16))
        self.text(right, "端口 8443 · HTTPS / WebRTC", 10, MUTED).pack(anchor="w")
        self.button(right, "启动手机服务", self.start_phone, True).pack(fill="x", pady=(18, 10))
        self.button(right, "断开当前手机", self.disconnect_phone).pack(fill="x", pady=5)
        self.button(right, "停止手机服务", self.stop_phone).pack(fill="x", pady=5)
        self.button(right, "打开证书文件夹", self.open_certs).pack(fill="x", pady=5)
        self.phone_status = self.text(right, "服务尚未启动", 11, CYAN, wraplength=350, justify="left")
        self.phone_status.pack(anchor="w", pady=20)
        self.text(right, "1  手机与电脑连接同一 Wi-Fi\n2  首次配置信任证书\n3  使用系统浏览器扫码并允许摄像头\n4  返回「实时采集」查看画面", 10, MUTED, justify="left", wraplength=360).pack(anchor="w", pady=6)

    def field(self, parent, label, variable, values):
        row = tk.Frame(parent, bg=PANEL)
        row.pack(fill="x", pady=9)
        self.text(row, label, 10, MUTED).pack(side="left")
        self.combo(row, variable, values, 17).pack(side="right")

    def _build_settings(self, parent):
        columns = tk.Frame(parent, bg=BG)
        columns.pack(fill="x")
        left = tk.Frame(columns, bg=PANEL, padx=22, pady=22)
        left.pack(side="left", fill="both", expand=True, padx=(0, 16))
        right = tk.Frame(columns, bg=PANEL, padx=22, pady=22)
        right.pack(side="left", fill="both", expand=True)
        self.text(left, "图像预处理", 17, bold=True).pack(anchor="w", pady=(0, 16))
        self.field(left, "输出尺寸", self.out_size, ["256 × 256", "640 × 640", "1280 × 720"])
        self.field(left, "顺时针旋转", self.rotation, ["0", "90", "180", "270"])
        self.field(left, "输出颜色格式", self.color, ["RGB", "BGR", "GRAY"])
        self.field(left, "补边灰度值", self.padding, ["0", "24", "114", "255"])
        ttk.Checkbutton(left, text="旋转后水平镜像", variable=self.mirror).pack(anchor="w", pady=(15, 8))
        ttk.Checkbutton(left, text="归一化为 float32 / 0–1", variable=self.normalize).pack(anchor="w", pady=8)
        self.text(left, "缩放保持宽高比，空余区域统一补边。\n输出附带坐标变换矩阵。", 10, MUTED, justify="left").pack(anchor="w", pady=(20, 0))
        self.text(right, "电脑采集参数", 17, bold=True).pack(anchor="w", pady=(0, 16))
        self.field(right, "目标分辨率", self.capture_size, ["640 × 480", "1280 × 720", "1920 × 1080"])
        self.field(right, "目标帧率", self.capture_fps, ["15", "20", "30", "60"])
        self.text(right, "采集参数在下次启动时生效。\n实际尺寸和帧率由设备能力决定。\n手机参数在手机采集页面设置。", 10, MUTED, justify="left", wraplength=310).pack(anchor="w", pady=24)
        self.text(parent, "算法模块读取最新帧即可，处理速度较慢时自动跳过旧帧，避免积压。", 10, MUTED, anchor="w").pack(fill="x", pady=22)
        self.button(parent, "保存并应用", self.apply_settings, True).pack(anchor="e")
        self.text(parent, "输出内容：图像数组、帧编号、会话编号、时间信息、原始尺寸和坐标变换。", 10, MUTED, anchor="w", wraplength=850).pack(fill="x", pady=20)

    @staticmethod
    def dimensions(text):
        return tuple(int(v.strip()) for v in text.split("×"))

    def configs(self):
        w, h = self.dimensions(self.capture_size.get())
        capture = CaptureConfig(w, h, int(self.capture_fps.get()))
        w, h = self.dimensions(self.out_size.get())
        processing = PreprocessConfig(w, h, int(self.rotation.get()), self.mirror.get(), self.color.get(), self.normalize.get(), int(self.padding.get()))
        capture.validate(); processing.validate()
        return capture, processing

    def _load_settings(self):
        try:
            data = json.loads((self.runtime / "settings.json").read_text(encoding="utf-8"))
            capture, processing = CaptureConfig(**data["capture"]), PreprocessConfig(**data["processing"])
            capture.validate(); processing.validate()
            self.capture_size.set(f"{capture.width} × {capture.height}"); self.capture_fps.set(str(capture.fps))
            self.out_size.set(f"{processing.width} × {processing.height}"); self.rotation.set(str(processing.rotation))
            self.color.set(processing.color); self.mirror.set(processing.mirror); self.normalize.set(processing.normalize)
            self.padding.set(str(processing.padding_value))
            self.engine.configure(processing)
        except (OSError, ValueError, KeyError, TypeError):
            pass

    def apply_settings(self):
        try:
            capture, processing = self.configs()
            self.engine.configure(processing)
            path = self.runtime / "settings.json"
            temporary = path.with_suffix(".tmp")
            temporary.write_text(json.dumps({"capture":asdict(capture),"processing":asdict(processing)}, ensure_ascii=False, indent=2), encoding="utf-8")
            temporary.replace(path)
            self.footer.configure(text="图像处理参数已应用；采集参数在下次启动时生效。", fg=CYAN)
        except Exception as exc:
            messagebox.showerror("配置无效", str(exc))

    def run_action(self, action, done=None):
        if self.busy:
            self.footer.configure(text="正在完成上一个操作，请稍候。")
            return
        self.busy = True
        self.footer.configure(text="正在处理…", fg=MUTED)
        def worker():
            try:
                result = action()
                self.events.put((done, result, None))
            except Exception as exc:
                self.events.put((None, None, exc))
        threading.Thread(target=worker, daemon=True).start()

    def choose_video(self):
        path = filedialog.askopenfilename(title="选择本地视频", filetypes=[("视频", "*.mp4 *.avi *.mov *.mkv"), ("所有文件", "*.*")])
        if path:
            self.file_var.set(path); self.source_var.set("视频文件"); self.file_label.configure(text=path)

    def start_local(self):
        try:
            capture, processing = self.configs()
            kind = self.source_var.get()
            source = "demo" if kind == "演示画面" else int(self.camera_var.get()) if kind == "电脑摄像头" else self.file_var.get()
            if kind == "视频文件" and not source:
                self.choose_video(); source = self.file_var.get()
                if not source: return
            self.engine.configure(processing)
            def action():
                if self.phone:
                    self.phone.stop(); self.phone = None
                self.engine.start(source, capture)
            self.run_action(action, lambda _:self.footer.configure(text="采集已启动", fg=CYAN))
            self.file_label.configure(text="演示画面：合成测试图案，未打开摄像头。" if source == "demo" else str(source) if isinstance(source, str) else f"电脑摄像头 · 设备 {source}")
        except Exception as exc:
            messagebox.showerror("无法开始", str(exc))

    def stop_capture(self):
        def action():
            if self.phone:
                self.phone.stop(); self.phone = None
            self.engine.stop()
        self.run_action(action, lambda _:self.footer.configure(text="采集已停止", fg=MUTED))

    def start_phone(self):
        host = self.host_var.get().strip()
        def action():
            try:
                from camera_module.phone import PhoneServer
            except ImportError as exc:
                raise RuntimeError("手机依赖未安装，请运行 setup.bat。") from exc
            if self.phone:
                self.phone.stop(); self.phone = None
            self.engine.stop()
            server = PhoneServer(self.engine, self.runtime, host)
            server.start()
            self.phone = server
            return server.url
        def done(url):
            import qrcode
            self.phone_url.set(url)
            image = qrcode.make(url).convert("RGB").resize((260,260), Image.Resampling.NEAREST)
            self._qr_photo = ImageTk.PhotoImage(image)
            self.qr_label.configure(image=self._qr_photo, text="", width=260, height=260)
            self.footer.configure(text="手机服务已启动。首次使用请先按 README 配置信任证书。", fg=CYAN)
        self.run_action(action, done)

    def disconnect_phone(self):
        def action():
            if self.phone:
                future = self.phone.disconnect()
                if future: future.result(timeout=8)
        self.run_action(action)

    def stop_phone(self):
        def action():
            if self.phone:
                self.phone.stop(); self.phone = None
                self.engine.stop()
        self.run_action(action)

    def open_certs(self):
        directory = self.runtime / "certs"
        if not directory.exists():
            messagebox.showinfo("证书尚未生成", "请先启动手机服务。")
        else:
            os.startfile(directory)

    def copy(self, text):
        self.root.clipboard_clear(); self.root.clipboard_append(text)
        self.footer.configure(text="连接地址已复制")

    def snapshot(self):
        packet = self.engine.latest()
        if packet is None or self.engine.stats()["status"] not in ("running", "ended"):
            messagebox.showinfo("暂无画面", "请先开始采集。")
            return
        self.run_action(lambda: save_frame(packet, self.runtime / "snapshots"),
                        lambda path: self.footer.configure(text=f"已保存原图、处理图、数组和元数据：{path}", fg=CYAN))

    def draw(self):
        if not hasattr(self, "canvas"): return
        width, height = self.canvas.winfo_width(), self.canvas.winfo_height()
        self.canvas.delete("all")
        if self.last_packet is None:
            self.canvas.create_text(width/2,height/2-18,text="等待输入画面",fill=TEXT,font=("Microsoft YaHei UI",22,"bold"))
            self.canvas.create_text(width/2,height/2+23,text="选择输入源并开始采集，或前往「手机连接」",fill=MUTED,font=("Microsoft YaHei UI",11))
            return
        packet = self.last_packet
        array = packet.image
        if packet.normalized: array = np.rint(array*255).clip(0,255).astype(np.uint8)
        if packet.color == "BGR": array = cv2.cvtColor(array,cv2.COLOR_BGR2RGB)
        if self.preview_original.get():
            array = cv2.cvtColor(packet.source_bgr, cv2.COLOR_BGR2RGB)
        self.preview_note.configure(text="原始画面 · 未旋转/镜像" if self.preview_original.get() else "预处理画面 · 保持宽高比并补边")
        image = Image.fromarray(array)
        scale = min(max(1, width - 16) / image.width, max(1, height - 16) / image.height)
        image = image.resize((max(1, round(image.width * scale)), max(1, round(image.height * scale))),
                             Image.Resampling.BILINEAR)
        self._photo = ImageTk.PhotoImage(image)
        self.canvas.create_image(width/2,height/2,image=self._photo)

    def tick(self):
        try:
            while True:
                done, result, error = self.events.get_nowait()
                self.busy = False
                if error:
                    self.footer.configure(text=str(error), fg="#ff8f94")
                    messagebox.showerror("操作未完成", str(error))
                elif done:
                    done(result)
        except queue.Empty:
            pass
        stats = self.engine.stats()
        self.side_status.configure(text="●  " + stats["message"])
        self.metrics["fps"].configure(text=f"{stats['fps']:.1f} fps")
        packet = self.engine.latest(after_id=self.last_id)
        if packet:
            self.last_packet, self.last_id = packet, packet.frame_id
            self.metrics["source"].configure(text=f"{packet.source_size[0]} × {packet.source_size[1]}")
            self.metrics["output"].configure(text=f"{packet.image.shape[1]} × {packet.image.shape[0]}")
            self.metrics["cost"].configure(text=f"{packet.processing_ms:.1f} ms")
            self.frame_note.configure(text=f"帧 #{packet.frame_id} · {packet.color} · {packet.image.dtype}")
            if self.current_page == "live": self.draw()
        elif stats["status"] in ("idle", "waiting", "opening", "error", "stopping") and self.last_packet is not None:
            self.last_packet = None; self.draw()
            for name in ("source", "output", "cost"): self.metrics[name].configure(text="—")
            self.frame_note.configure(text="尚无图像帧")
        if self.phone:
            self.phone_status.configure(text=self.phone.message)
        else:
            self.phone_status.configure(text="服务尚未启动")
            if self.phone_url.get() != "尚未启动":
                self.phone_url.set("尚未启动")
                self.qr_label.configure(image="", text="启动手机服务后\n在这里显示配对二维码", width=28, height=14)
        self.root.after(40, self.tick)

    def close(self):
        if self.busy:
            self.footer.configure(text="正在完成操作，随后自动关闭。")
            self.root.after(100, self.close)
            return
        def action():
            if self.phone: self.phone.stop()
            self.engine.stop()
        self.run_action(action, lambda _:self.root.destroy())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="PoseLab 摄像头与图像处理")
    parser.add_argument("--demo", action="store_true", help="启动合成演示，不访问摄像头")
    args = parser.parse_args()
    if os.name == "nt":
        import ctypes
        try: ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception: pass
    root = create_root()
    app = App(root, demo=args.demo)
    root.mainloop()
