# =============================================================================
# PatchRaylibAndroid.cmake — Vá raylib 5.5 để hỗ trợ XOAY MÀN HÌNH trên Android.
#
# Vấn đề: raylib 5.5 cố định kích thước bộ đệm khung hình (ANativeWindow_setBuffersGeometry)
# lúc khởi động và bỏ qua sự kiện đổi kích thước cửa sổ. Khi xoay điện thoại dọc ↔ ngang,
# hệ điều hành kéo giãn khung hình cũ cho vừa màn hình mới → giao diện bị méo, chạm lệch.
#
# Bản vá: theo dõi kích thước thật của cửa sổ (ANativeWindow_getWidth/Height); khi thay đổi
# thì tạo lại EGL surface với kích thước mới và cập nhật kích thước màn hình/viewport nội bộ
# của raylib → GetScreenWidth()/GetScreenHeight() trả về đúng, game tự bố trí lại giao diện.
#
# Chạy bởi FetchContent (PATCH_COMMAND) với thư mục làm việc = mã nguồn raylib. Idempotent.
# =============================================================================
set(file "src/platforms/rcore_android.c")
file(READ "${file}" content)
if (content MATCHES "WB_RESIZE_PATCH")
    message(STATUS "raylib: bản vá xoay màn hình Android đã có")
    return()
endif()

set(poll_anchor "// Register all input events\nvoid PollInputEvents(void)\n{\n")
set(cfg_anchor  "        case APP_CMD_CONFIG_CHANGED:\n        {\n")
string(FIND "${content}" "${poll_anchor}" p1)
string(FIND "${content}" "${cfg_anchor}" p2)
if (p1 EQUAL -1 OR p2 EQUAL -1)
    message(FATAL_ERROR "PatchRaylibAndroid: không tìm thấy điểm vá trong ${file} (phiên bản raylib khác 5.5?)")
endif()

set(resize_fn [=[
// [WB_RESIZE_PATCH] Đổi kích thước cửa sổ (xoay màn hình) — raylib 5.5 chưa tự xử lý
static int wbResizeCountdown = 0;
static void WbResizeSurface(void)
{
    if ((platform.app == NULL) || (platform.app->window == NULL) || (platform.surface == EGL_NO_SURFACE) || !CORE.Window.ready) return;
    int w = ANativeWindow_getWidth(platform.app->window);
    int h = ANativeWindow_getHeight(platform.app->window);
    if ((w <= 0) || (h <= 0) || ((w == CORE.Window.screen.width) && (h == CORE.Window.screen.height))) return;

    EGLint displayFormat = 0;
    eglGetConfigAttrib(platform.device, platform.config, EGL_NATIVE_VISUAL_ID, &displayFormat);
    // Tạo lại surface với bộ đệm kích thước mới (context OpenGL giữ nguyên → texture không mất)
    eglMakeCurrent(platform.device, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(platform.device, platform.surface);
    ANativeWindow_setBuffersGeometry(platform.app->window, w, h, displayFormat);
    platform.surface = eglCreateWindowSurface(platform.device, platform.config, platform.app->window, NULL);
    eglMakeCurrent(platform.device, platform.surface, platform.surface, platform.context);

    CORE.Window.display.width = w;   CORE.Window.display.height = h;
    CORE.Window.screen.width = w;    CORE.Window.screen.height = h;
    CORE.Window.render.width = w;    CORE.Window.render.height = h;
    CORE.Window.renderOffset.x = 0;  CORE.Window.renderOffset.y = 0;
    CORE.Window.screenScale = MatrixIdentity();
    CORE.Window.currentFbo.width = w; CORE.Window.currentFbo.height = h;
    rlSetFramebufferWidth(w);
    rlSetFramebufferHeight(h);
    SetupViewport(w, h);
    CORE.Window.resizedLastFrame = true;
    TRACELOG(LOG_INFO, "DISPLAY: Window resized to %i x %i (WB_RESIZE_PATCH)", w, h);
}

]=])
set(poll_hook [=[
    // [WB_RESIZE_PATCH] sau sự kiện xoay/đổi cỡ: kiểm tra liên tục ~1.5 giây; ngoài ra kiểm tra thưa mỗi 30 khung
    static int wbFrame = 0;
    if ((wbResizeCountdown > 0) || ((++wbFrame % 30) == 0))
    {
        if (wbResizeCountdown > 0) wbResizeCountdown--;
        WbResizeSurface();
    }

]=])
set(cfg_hook [=[
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_CONFIG_CHANGED:
        {
            wbResizeCountdown = 90;   // [WB_RESIZE_PATCH]
]=])

string(REPLACE "${poll_anchor}" "${resize_fn}${poll_anchor}${poll_hook}" content "${content}")
string(REPLACE "${cfg_anchor}" "${cfg_hook}" content "${content}")
file(WRITE "${file}" "${content}")
message(STATUS "raylib: đã vá hỗ trợ xoay màn hình Android (${file})")
