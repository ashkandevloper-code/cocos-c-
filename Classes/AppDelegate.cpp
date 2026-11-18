#include "AppDelegate.h"

#include "app/NxApp.h"
#include "audio/include/AudioEngine.h"
#include "scenes/ShellScenes.h"

USING_NS_CC;

// Portrait phone design resolution (Flutter app is portrait-only).
static const cocos2d::Size kDesign(720, 1280);

AppDelegate::AppDelegate() {}

AppDelegate::~AppDelegate() { AudioEngine::end(); }

void AppDelegate::initGLContextAttrs() {
    GLContextAttrs attrs = {8, 8, 8, 8, 24, 8, 0};
    GLView::setGLContextAttrs(attrs);
}

bool AppDelegate::applicationDidFinishLaunching() {
    auto director = Director::getInstance();
    auto glview = director->getOpenGLView();
    if (!glview) {
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WIN32) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC) || (CC_TARGET_PLATFORM == CC_PLATFORM_LINUX)
        glview = GLViewImpl::createWithRect("NEXORA", cocos2d::Rect(0, 0, 450, 800));
#else
        glview = GLViewImpl::create("NEXORA");
#endif
        director->setOpenGLView(glview);
    }
    director->setAnimationInterval(1.0f / 60);
    glview->setDesignResolutionSize(kDesign.width, kDesign.height, ResolutionPolicy::FIXED_WIDTH);
    director->setClearColor(Color4F(7 / 255.f, 3 / 255.f, 26 / 255.f, 1));
    FileUtils::getInstance()->addSearchPath("res");

    nx::NxApp::get().init();
    auto& st = nx::NxApp::get().settings();
    if (st.music()) {
        int id = AudioEngine::play2d("media/bgm_main.ogg", true, static_cast<float>(st.musicVolume()));
        UserDefault::getInstance()->setIntegerForKey("nx.bgm.id", id);
    }
    director->runWithScene(nx::IntroScene::create());
    return true;
}

void AppDelegate::applicationDidEnterBackground() {
    Director::getInstance()->stopAnimation();
    AudioEngine::pauseAll();
}

void AppDelegate::applicationWillEnterForeground() {
    Director::getInstance()->startAnimation();
    AudioEngine::resumeAll();
}
