// Minecraft Axeolon for Android: a NativeActivity. The game itself runs on its own thread
// (android/src/Platform.cpp): the same main loop as the 3DS (source/main.cpp) on OpenGL ES,
// AAudio and Pocket Edition's touch controls. This thread hands it the window and the fingers.
#include <android/configuration.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <chrono>
#include <thread>
#include <vector>
#include "Platform.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "axeolon", __VA_ARGS__)

namespace {

// Immersive full screen: status and navigation bars hidden (a swipe brings them back for a moment),
// drawing right into the corners and under the camera cutout - no black bars on any phone shape.
void hideSystemBars(ANativeActivity* act) {
    JNIEnv* env = nullptr;
    if (act->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return;
    jclass actCls = env->GetObjectClass(act->clazz);
    jobject window = env->CallObjectMethod(act->clazz, env->GetMethodID(actCls, "getWindow", "()Landroid/view/Window;"));
    if (window) {
        jclass winCls = env->GetObjectClass(window);
        jmethodID fits = env->GetMethodID(winCls, "setDecorFitsSystemWindows", "(Z)V");
        if (fits) env->CallVoidMethod(window, fits, JNI_FALSE);
        else env->ExceptionClear();
        jmethodID getCtl = env->GetMethodID(winCls, "getInsetsController", "()Landroid/view/WindowInsetsController;");
        jobject ctl = getCtl ? env->CallObjectMethod(window, getCtl) : nullptr;
        if (!getCtl) env->ExceptionClear();
        if (ctl) {
            jclass typeCls = env->FindClass("android/view/WindowInsets$Type");
            jint bars = env->CallStaticIntMethod(typeCls, env->GetStaticMethodID(typeCls, "systemBars", "()I"));
            jclass ctlCls = env->GetObjectClass(ctl);
            env->CallVoidMethod(ctl, env->GetMethodID(ctlCls, "hide", "(I)V"), bars);
            env->CallVoidMethod(ctl, env->GetMethodID(ctlCls, "setSystemBarsBehavior", "(I)V"), 2);
        } else {
            jobject decor = env->CallObjectMethod(window, env->GetMethodID(winCls, "getDecorView", "()Landroid/view/View;"));
            if (decor) {
                jclass viewCls = env->GetObjectClass(decor);
                env->CallVoidMethod(decor, env->GetMethodID(viewCls, "setSystemUiVisibility", "(I)V"), 0x1706);
            }
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    act->vm->DetachCurrentThread();
}

void onCmd(android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (!app->window) break;
        hideSystemBars(app->activity);
        platform::setWindow(app->window);
        if (!platform::gameRunning()) {
            AConfiguration* cfg = AConfiguration_new();
            AConfiguration_fromAssetManager(cfg, app->activity->assetManager);
            platform::setAssets(app->activity->assetManager, app->activity->internalDataPath, float(AConfiguration_getDensity(cfg)));
            AConfiguration_delete(cfg);
            platform::startGame();
        }
        break;
    case APP_CMD_TERM_WINDOW: platform::setWindow(nullptr); break;
    case APP_CMD_GAINED_FOCUS: hideSystemBars(app->activity); break;
    case APP_CMD_PAUSE:
    case APP_CMD_SAVE_STATE: platform::requestSave(); break;
    default: break;
    }
}

// the character a key makes (KeyEvent.getUnicodeChar)
unsigned unicodeOf(ANativeActivity* act, AInputEvent* ev) {
    JNIEnv* env = nullptr;
    if (act->vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) return 0;
    jclass cls = env->FindClass("android/view/KeyEvent");
    jobject ke = env->NewObject(cls, env->GetMethodID(cls, "<init>", "(II)V"), AKeyEvent_getAction(ev), AKeyEvent_getKeyCode(ev));
    jint c = env->CallIntMethod(ke, env->GetMethodID(cls, "getUnicodeChar", "(I)I"), AKeyEvent_getMetaState(ev));
    if (env->ExceptionCheck()) { env->ExceptionClear(); c = 0; }
    act->vm->DetachCurrentThread();
    return unsigned(c);
}

android_app* gApp = nullptr;
bool gKeyboardShown = false;

int32_t onInput(android_app*, AInputEvent* ev) {
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        if (platform::typingWanted() && AKeyEvent_getKeyCode(ev) != AKEYCODE_BACK) {
            if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_DOWN) {
                int code = AKeyEvent_getKeyCode(ev);
                if (code == AKEYCODE_DEL) platform::typeChar(8);
                else if (code == AKEYCODE_ENTER) platform::typeChar('\n');
                else if (unsigned c = unicodeOf(gApp->activity, ev)) platform::typeChar(c);
            }
            return 1;
        }
        if (AKeyEvent_getKeyCode(ev) != AKEYCODE_BACK) return 0;
        if (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_UP) platform::backPressed();
        return 1; // the game decides what back does (pause, close the inventory)
    }
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t action = AMotionEvent_getAction(ev), kind = action & AMOTION_EVENT_ACTION_MASK;
    size_t idx = size_t((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    float s = 1.0f;
    (void)s;
    switch (kind) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
        platform::touchEvent(platform::TOUCH_DOWN, AMotionEvent_getPointerId(ev, idx), AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        for (size_t i = 0; i < AMotionEvent_getPointerCount(ev); ++i)
            platform::touchEvent(platform::TOUCH_MOVE, AMotionEvent_getPointerId(ev, i), AMotionEvent_getX(ev, i), AMotionEvent_getY(ev, i));
        break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
        platform::touchEvent(platform::TOUCH_UP, AMotionEvent_getPointerId(ev, idx), AMotionEvent_getX(ev, idx), AMotionEvent_getY(ev, idx));
        break;
    case AMOTION_EVENT_ACTION_CANCEL: platform::touchEvent(platform::TOUCH_CANCEL, -1, 0, 0); break;
    default: break;
    }
    return 1;
}

} // namespace

void android_main(android_app* app) {
    gApp = app;
    app->onAppCmd = onCmd;
    app->onInputEvent = onInput;
    while (true) {
        int events = 0;
        android_poll_source* source = nullptr;
        // wakes at least every 100 ms, to show or hide the keyboard when the game wants text
        int r = ALooper_pollOnce(100, nullptr, &events, reinterpret_cast<void**>(&source));
        bool want = platform::typingWanted();
        if (want != gKeyboardShown) {
            if (want) ANativeActivity_showSoftInput(app->activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
            else ANativeActivity_hideSoftInput(app->activity, 0);
            gKeyboardShown = want;
        }
        if (r >= 0 && source) source->process(app, source);
        if (app->destroyRequested) {
            platform::requestQuit();
            // let the game save before the process goes
            for (int i = 0; i < 100 && platform::gameRunning(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return;
        }
    }
}
