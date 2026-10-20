#include "engine.h"

#include <jni.h>

#include <exception>
#include <string>

namespace
{
std::string Utf8(JNIEnv* env, jstring value)
{
  const char* chars = env->GetStringUTFChars(value, nullptr);
  if (chars == nullptr)
    return {};
  std::string out(chars);
  env->ReleaseStringUTFChars(value, chars);
  return out;
}
} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_volum_engine_NativeEngine_render(JNIEnv* env, jobject, jstring input, jstring rig, jstring output)
{
  try
  {
    const auto result =
      volum::android::engine::RenderFixedChain(Utf8(env, input), Utf8(env, rig), Utf8(env, output));
    return env->NewStringUTF(volum::android::engine::ResultJson(result).c_str());
  }
  catch (const std::exception& error)
  {
    env->ThrowNew(env->FindClass("java/lang/RuntimeException"), error.what());
    return nullptr;
  }
}
