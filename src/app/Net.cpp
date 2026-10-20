// =============================================================================
// Net.cpp — xem Net.h
// =============================================================================
#include "Net.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#if defined(PLATFORM_ANDROID)
#include <jni.h>
#include <android_native_app_glue.h>
extern "C" struct android_app* GetAndroidApp(void);   // raylib
#endif

#ifndef WB_LIVE_WORLD_URL
#define WB_LIVE_WORLD_URL "https://github.com/quocdat16610-web/ai-box/releases/download/live-world/world.wbsave"
#endif
#ifndef WB_LIVE_WORLD_PAGE
#define WB_LIVE_WORLD_PAGE "https://github.com/quocdat16610-web/ai-box/releases/tag/live-world"
#endif

namespace wb {

const char* liveWorldUrl() {
    const char* o = std::getenv("WB_LIVE_URL");   // thử nghiệm: ghi đè địa chỉ (vd. file:///...)
    return o && *o ? o : WB_LIVE_WORLD_URL;
}
const char* liveWorldPage() { return WB_LIVE_WORLD_PAGE; }

Download::~Download() {
    if (th_.joinable()) th_.join();
}

void Download::start(const std::string& url, const std::string& path) {
    if (state_ == Running) return;
    if (th_.joinable()) th_.join();
    state_ = Running;
    bytes_ = 0;
    { std::lock_guard<std::mutex> l(m_); error_.clear(); }
    th_ = std::thread(&Download::run, this, url, path);
}

namespace {
bool fileSize(const std::string& p, long& n) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    n = std::ftell(f);
    std::fclose(f);
    return true;
}

#if defined(PLATFORM_ANDROID)
// Tải bằng HttpURLConnection (tự đi theo chuyển hướng https → https của GitHub Releases)
bool androidFetch(const std::string& url, const std::string& tmp, std::string& err, std::atomic<long>& bytes) {
    android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->vm) { err = "Không truy cập được Java VM"; return false; }
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) { err = "AttachCurrentThread thất bại"; return false; }
    bool ok = false;
    auto failed = [&](const char* what) {
        if (env->ExceptionCheck()) { env->ExceptionClear(); }
        err = what;
        return false;
    };
    do {
        jclass cURL = env->FindClass("java/net/URL");
        if (!cURL) { failed("Thiếu java.net.URL"); break; }
        jmethodID ctor = env->GetMethodID(cURL, "<init>", "(Ljava/lang/String;)V");
        jstring jurl = env->NewStringUTF(url.c_str());
        jobject u = env->NewObject(cURL, ctor, jurl);
        if (env->ExceptionCheck() || !u) { failed("Địa chỉ không hợp lệ"); break; }
        jobject conn = env->CallObjectMethod(u, env->GetMethodID(cURL, "openConnection", "()Ljava/net/URLConnection;"));
        if (env->ExceptionCheck() || !conn) { failed("Không mở được kết nối (kiểm tra mạng)"); break; }
        jclass cConn = env->FindClass("java/net/HttpURLConnection");
        env->CallVoidMethod(conn, env->GetMethodID(cConn, "setInstanceFollowRedirects", "(Z)V"), JNI_TRUE);
        env->CallVoidMethod(conn, env->GetMethodID(cConn, "setConnectTimeout", "(I)V"), 15000);
        env->CallVoidMethod(conn, env->GetMethodID(cConn, "setReadTimeout", "(I)V"), 30000);
        jint code = env->CallIntMethod(conn, env->GetMethodID(cConn, "getResponseCode", "()I"));
        if (env->ExceptionCheck()) { failed("Không kết nối được tới GitHub (kiểm tra mạng)"); break; }
        if (code != 200) { err = "Máy chủ trả mã " + std::to_string(code) + (code == 404 ? " — thế giới trên GitHub chưa được tạo" : ""); break; }
        jobject in = env->CallObjectMethod(conn, env->GetMethodID(cConn, "getInputStream", "()Ljava/io/InputStream;"));
        if (env->ExceptionCheck() || !in) { failed("Không đọc được dữ liệu"); break; }
        jclass cIn = env->FindClass("java/io/InputStream");
        jmethodID read = env->GetMethodID(cIn, "read", "([B)I");
        jbyteArray buf = env->NewByteArray(65536);
        FILE* f = std::fopen(tmp.c_str(), "wb");
        if (!f) { err = "Không ghi được tệp tạm"; break; }
        std::vector<jbyte> chunk(65536);
        bool readOk = true;
        for (;;) {
            jint n = env->CallIntMethod(in, read, buf);
            if (env->ExceptionCheck()) { env->ExceptionClear(); readOk = false; err = "Mất kết nối khi đang tải"; break; }
            if (n < 0) break;
            env->GetByteArrayRegion(buf, 0, n, chunk.data());
            if (std::fwrite(chunk.data(), 1, static_cast<size_t>(n), f) != static_cast<size_t>(n)) { readOk = false; err = "Hết bộ nhớ máy?"; break; }
            bytes += n;
        }
        std::fclose(f);
        env->CallVoidMethod(in, env->GetMethodID(cIn, "close", "()V"));
        if (env->ExceptionCheck()) env->ExceptionClear();
        ok = readOk;
    } while (false);
    vm->DetachCurrentThread();
    return ok;
}
#endif
} // namespace

void Download::run(std::string url, std::string path) {
    std::string tmp = path + ".download";
    std::string err;
    bool ok = false;
#if defined(PLATFORM_ANDROID)
    ok = androidFetch(url, tmp, err, bytes_);
#else
    // curl có sẵn trên Windows 10 (1803+), Linux và macOS
#if defined(_WIN32)
    std::string cmd = "curl.exe -L --fail -sS --connect-timeout 15 -o \"" + tmp + "\" \"" + url + "\" 2>NUL";
#else
    std::string cmd = "curl -L --fail -sS --connect-timeout 15 -o '" + tmp + "' '" + url + "' 2>/dev/null";
#endif
    int rc = std::system(cmd.c_str());
#if !defined(_WIN32)
    if (rc > 255) rc >>= 8;   // mã thoát thật của curl
#endif
    ok = rc == 0;
    if (rc == 22) err = "GitHub chưa có thế giới (lỗi 404) — chạy workflow \"Thế giới đang sống\" trên GitHub trước";
    else if (rc == 6 || rc == 7 || rc == 28) err = "Không kết nối được tới GitHub — kiểm tra mạng";
    else if (rc == 127 || rc == 9009) err = "Máy chưa có lệnh curl (cần Windows 10 trở lên)";
    else if (!ok) err = "Tải thất bại (curl mã " + std::to_string(rc) + ")";
#endif
    long n = 0;
    if (ok && (!fileSize(tmp, n) || n < 64)) { ok = false; err = "Tệp tải về rỗng"; }
    if (ok) {
        bytes_ = n;
        std::remove(path.c_str());
        if (std::rename(tmp.c_str(), path.c_str()) != 0) { ok = false; err = "Không lưu được tệp"; }
    } else {
        std::remove(tmp.c_str());
    }
    { std::lock_guard<std::mutex> l(m_); error_ = err; }
    state_ = ok ? Done : Failed;
}

} // namespace wb
