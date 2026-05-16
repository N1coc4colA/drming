#include "native.h"

#include <QDebug>

#ifdef Q_OS_ANDROID

#include <QJniEnvironment>
#include <QJniObject>

#include "mdnsmanager.h"

void onServiceFound(JNIEnv *env, jclass clazz, jstring jname, jstring jtype)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *type = env->GetStringUTFChars(jtype, nullptr);

    MdnsManager::instance().onServiceFound(QString::fromUtf8(name), QString::fromUtf8(type));

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jtype, type);
}

void onServiceLost(JNIEnv *env, jclass clazz, jstring jname, jstring jip)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *ip = env->GetStringUTFChars(jip, nullptr);

    MdnsManager::instance().onServiceLost(QString::fromUtf8(name), QString::fromUtf8(ip));

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jip, ip);
}

void onServiceResolved(JNIEnv *env, jclass clazz, jstring jname, jstring jhost, jstring jip, jint port)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *ip = env->GetStringUTFChars(jip, nullptr);
    const char *host = env->GetStringUTFChars(jhost, nullptr);

    MdnsManager::instance().onServiceResolved(QString::fromUtf8(name), QString::fromUtf8(host), QString::fromUtf8(ip), port);

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jhost, host);
    env->ReleaseStringUTFChars(jip, ip);
}

void onConnectivityChanged(JNIEnv *env, jclass clazz, jboolean connected)
{
    Q_UNUSED(clazz);

    MdnsManager::instance().networkState().onConnectivityChanged(connected);
}

bool registerNativeMethods_MdnsHelper(QJniObject &m_javaHelper)
{
    JNIEnv *env = QJniEnvironment().jniEnv();
    if (!env) {
        return false;
    }

    // Get the class from the already-loaded object, not FindClass
    jclass clazz = env->GetObjectClass(m_javaHelper.object<jobject>());
    if (!clazz) {
        return false;
    }

    const std::vector<JNINativeMethod> methods = {{"nativeOnServiceFound", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) ::onServiceFound},
                                                  {"nativeOnServiceLost", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) ::onServiceLost},
                                                  {"nativeOnServiceResolved",
                                                   "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V",
                                                   (void *) ::onServiceResolved}};
    const jint regResult = env->RegisterNatives(clazz, methods.data(), methods.size());
    if (regResult != 0) {
        qWarning() << "Failed to register native methods for MdnsHelper:" << regResult;
        env->DeleteLocalRef(clazz);
        return false;
    }

    qDebug() << "Registered native methods for MdnsHelper";
    env->DeleteLocalRef(clazz);

    return true;
}

bool registerNativeMethods_NetworkHelper(QJniObject &m_javaHelper)
{
    JNIEnv *env = QJniEnvironment().jniEnv();
    if (!env) {
        return false;
    }

    // Get the class from the already-loaded object, not FindClass
    jclass clazz = env->GetObjectClass(m_javaHelper.object<jobject>());
    if (!clazz) {
        return false;
    }

    const std::vector<JNINativeMethod> methods = {{"nativeOnConnectivityChanged", "(Z)V", (void *) ::onConnectivityChanged}};
    const jint regResult = env->RegisterNatives(clazz, methods.data(), methods.size());
    if (regResult != 0) {
        qWarning() << "Failed to register native methods for NetworkHelper:" << regResult;
        env->DeleteLocalRef(clazz);
        return false;
    }

    qDebug() << "Registered native methods for NetworkHelper";
    env->DeleteLocalRef(clazz);

    return true;
}

bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper)
{
    if (m_javaHelper.isValid()) {
        return true;
    }

    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) {
        qWarning() << "Failed to get Android activity";
        return false;
    }

    m_javaHelper = QJniObject("eu/n1coc4cola/drming/MdnsHelper", "(Landroid/content/Context;)V", activity.object<jobject>());

    if (!m_javaHelper.isValid()) {
        qWarning() << "Failed to create MdnsHelper instance";
        return false;
    }

    // Register AFTER we have a live object, so GetObjectClass works
    if (!registerNativeMethods_MdnsHelper(m_javaHelper)) {
        qWarning() << "Failed to register native methods";
        return false;
    }

    return true;
}

bool createNativeObject_NetworkHelper(QJniObject &m_javaHelper)
{
    if (m_javaHelper.isValid()) {
        return true;
    }

    QJniObject activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) {
        qWarning() << "Failed to get Android activity";
        return false;
    }

    m_javaHelper = QJniObject("eu/n1coc4cola/drming/NetworkHelper", "(Landroid/content/Context;)V", activity.object<jobject>());

    if (!m_javaHelper.isValid()) {
        qWarning() << "Failed to create NetworkHelper instance";
        return false;
    }

    // Register AFTER we have a live object, so GetObjectClass works
    if (!registerNativeMethods_NetworkHelper(m_javaHelper)) {
        qWarning() << "Failed to register native methods";
        return false;
    }

    return true;
}

#else

bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper)
{
    Q_UNUSED(m_javaHelper);
    return false;
}

bool createNativeObject_NetworkHelper(QJniObject &m_javaHelper)
{
    Q_UNUSED(m_javaHelper);
    return false;
}

#endif
