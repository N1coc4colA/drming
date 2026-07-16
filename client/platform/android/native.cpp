#include "native.h"

#include <QDebug>
#include <QJniArray>
#include <QJniEnvironment>
#include <QJniObject>
#include <QTimeZone>

#include "../../mdns.h"
#include "../../networkstatus.h"

#include "fileprovider.h"

template<typename T>
class JavaProxy
{
public:
    explicit JavaProxy(T &instance)
        : m_instance(instance)
    {}

    T &operator->() { return m_instance; }

    auto &object() { return m_instance.m_objectHelper; }

private:
    T &m_instance;
};

void onServiceFound(JNIEnv *env, const jclass clazz, const jstring jname, const jstring jtype, const jstring jprotocol)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *type = env->GetStringUTFChars(jtype, nullptr);
    const char *protocol = env->GetStringUTFChars(jprotocol, nullptr);

    Mdns::instance()->onServiceFound(QString::fromUtf8(name), QString::fromUtf8(type), QString::fromUtf8(protocol));

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jtype, type);
}

void onServiceLost(JNIEnv *env, const jclass clazz, const jstring jname, const jstring jip, const jstring jprotocol)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *ip = env->GetStringUTFChars(jip, nullptr);
    const char *protocol = env->GetStringUTFChars(jprotocol, nullptr);

    Mdns::instance()->onServiceLost(QString::fromUtf8(name), QString::fromUtf8(ip), QString::fromUtf8(protocol));

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jip, ip);
}

void onServiceResolved(
    JNIEnv *env, const jclass clazz, const jstring jname, const jstring jhost, const jstring jip, const jint port, const jstring jprotocol)
{
    Q_UNUSED(clazz);

    const char *name = env->GetStringUTFChars(jname, nullptr);
    const char *ip = env->GetStringUTFChars(jip, nullptr);
    const char *host = env->GetStringUTFChars(jhost, nullptr);
    const char *protocol = env->GetStringUTFChars(jprotocol, nullptr);

    Mdns::instance()->onServiceResolved(QString::fromUtf8(name), QString::fromUtf8(host), QString::fromUtf8(ip), port, QString::fromUtf8(protocol));

    env->ReleaseStringUTFChars(jname, name);
    env->ReleaseStringUTFChars(jhost, host);
    env->ReleaseStringUTFChars(jip, ip);
}

void onConnectivityChanged(const JNIEnv *env, const jclass clazz, const jboolean connected)
{
    Q_UNUSED(env);
    Q_UNUSED(clazz);

    NetworkState::instance()->onConnectivityChanged(connected);
}

template<>
class JavaProxy<Platform::FileProvider>
{
    using T = Platform::FileProvider;

public:
    explicit JavaProxy(T &instance)
        : m_instance(instance)
    {}

    T &operator->() { return m_instance; }

    auto &object() { return m_instance.m_object; }

    void setup()
    {
        auto &instance = m_instance.m_object;
        m_instance.jServerCertsPath = [&instance]() -> QString { return instance.callObjectMethod<jstring>("getServerCertsPath").toString(); };
        m_instance.jClientPath = [&instance]() -> QString { return instance.callObjectMethod<jstring>("getClientPath").toString(); };
        m_instance.jClientCertName = [&instance]() -> QString { return instance.callObjectMethod<jstring>("getClientCertName").toString(); };
        m_instance.jClientKeyName = [&instance]() -> QString { return instance.callObjectMethod<jstring>("getClientKeyName").toString(); };

        m_instance.jServerCerts = [&instance]() -> FilesModel::MapType {
            FilesModel::MapType out{};
            const auto list = instance.callObjectMethod("getServerCerts", "()Ljava/util/List;");
            const jint size = list.callMethod<jint>("size");
            out.reserve(size);

            for (jint i = 0; i < size; ++i) {
                auto obj = list.callObjectMethod<jobject, jint>("get", static_cast<jint>(i));
                const long lastModified = obj.getField<jlong>("lastModified");
                const auto name = obj.getObjectField<jstring>("name").toString();

                out.append({QDateTime::fromMSecsSinceEpoch(lastModified, QTimeZone::UTC), name, {}});
            }

            return out;
        };
        m_instance.jClients = [&instance]() -> FilesModel::MapType {
            const auto list = instance.callObjectMethod("getClients", "()Ljava/util/List;");
            const jint size = list.callMethod<jint>("size");
            FilesModel::MapType out{};
            out.reserve(size);

            for (jint i = 0; i < size; ++i) {
                const auto obj = list.callObjectMethod<jobject, jint>("get", static_cast<jint>(i));
                const long lastModified = obj.getField<jlong>("lastModified");
                const auto name = obj.getObjectField<jstring>("name").toString();
                const auto hasCert = obj.getField<jboolean>("hasCert");
                const auto hasKey = obj.getField<jboolean>("hasKey");

                out.append({QDateTime::fromMSecsSinceEpoch(lastModified, QTimeZone::UTC), name, {{"cert", hasCert}, {"key", hasKey}}});
            }

            return out;
        };

        m_instance.jDeleteServerCert = [&instance](const QString &a) -> bool {
            const auto ja = QJniObject::fromString(a);
            return instance.callMethod<jboolean, jstring>("deleteServerCert", ja.object<jstring>());
        };
        m_instance.jDeleteClient = [&instance](const QString &a) -> bool {
            const auto ja = QJniObject::fromString(a);
            return instance.callMethod<jboolean, jstring>("deleteClient", ja.object<jstring>());
        };
        m_instance.jAddServerCert = [&instance](const QString &a) -> int {
            const auto ja = QJniObject::fromString(a);
            return instance.callMethod<jint, jstring>("addServerCert", ja.object<jstring>());
        };
        m_instance.jAddClientCert = [&instance](const QString &a, const QString &b) -> int {
            const auto ja = QJniObject::fromString(a), jb = QJniObject::fromString(b);
            return instance.callMethod<jint, jstring, jstring>("addClientCert", ja.object<jstring>(), jb.object<jstring>());
        };
        m_instance.jAddClientKey = [&instance](const QString &a, const QString &b) -> int {
            const auto ja = QJniObject::fromString(a), jb = QJniObject::fromString(b);
            return instance.callMethod<jint, jstring, jstring>("addClientKey", ja.object<jstring>(), jb.object<jstring>());
        };

        m_instance.jClientCertData = [&instance](const QString &a) -> QByteArray {
            const auto ja = QJniObject::fromString(a);
            return QJniArray<jbyte>(instance.callObjectMethod<jbyte[]>("getClientCertData", ja.object<jstring>())).toContainer();
        };
        m_instance.jClientKeyData = [&instance](const QString &a) -> QByteArray {
            const auto ja = QJniObject::fromString(a);
            return QJniArray<jbyte>(instance.callObjectMethod<jbyte[]>("getClientKeyData", ja.object<jstring>())).toContainer();
        };

        m_instance.jTrustedCertsData = [&instance]() -> QList<QByteArray> {
            const auto list = QJniObject(instance.callObjectMethod("getTrustedCertsData", "()Ljava/util/ArrayList;"));
            const jint size = list.callMethod<jint>("size");
            QList<QByteArray> out{};
            out.reserve(size);

            for (jint i = 0; i < size; i++) {
                out.append(QJniArray<jbyte>(list.callObjectMethod("get", "(I)Ljava/lang/Object;", i)).toContainer());
            }

            return out;
        };

        m_instance.jUpdateClientEntry = [&instance](const QString &a, const QString &b) -> bool {
            const auto ja = QJniObject::fromString(a), jb = QJniObject::fromString(b);
            return instance.callMethod<jboolean, jstring, jstring>("updateClientEntry", ja.object<jstring>(), jb.object<jstring>());
        };
        m_instance.jValidClientEntries = [&instance]() -> QList<QString> {
            return QJniArray<jstring>(instance.callObjectMethod<jstring[]>("getValidClientEntries")).toContainer();
        };
    }

private:
    T &m_instance;
};

bool registerNativeMethods_MdnsHelper(const QJniObject &m_javaHelper)
{
    const auto env = QJniEnvironment().jniEnv();
    if (!env) {
        return false;
    }

    // Get the class from the already-loaded object, not FindClass
    const auto clazz = env->GetObjectClass(m_javaHelper.object<jobject>());
    if (!clazz) {
        return false;
    }

    const std::vector<JNINativeMethod> methods
        = {{"nativeOnServiceFound", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void *>(onServiceFound)},
           {"nativeOnServiceLost", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", reinterpret_cast<void *>(onServiceLost)},
           {"nativeOnServiceResolved",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V",
            reinterpret_cast<void *>(onServiceResolved)}};
    const auto regResult = env->RegisterNatives(clazz, methods.data(), static_cast<jint>(methods.size()));
    if (regResult != 0) {
        qWarning() << "Failed to register native methods for MdnsHelper:" << regResult;
        env->DeleteLocalRef(clazz);
        return false;
    }

    qDebug() << "Registered native methods for MdnsHelper";
    env->DeleteLocalRef(clazz);

    return true;
}

bool registerNativeMethods_NetworkHelper(const QJniObject &m_javaHelper)
{
    const auto env = QJniEnvironment().jniEnv();
    if (!env) {
        return false;
    }

    // Get the class from the already-loaded object, not FindClass
    const auto clazz = env->GetObjectClass(m_javaHelper.object<jobject>());
    if (!clazz) {
        return false;
    }

    const std::vector<JNINativeMethod> methods = {{"nativeOnConnectivityChanged", "(Z)V", reinterpret_cast<void *>(onConnectivityChanged)}};
    const auto regResult = env->RegisterNatives(clazz, methods.data(), static_cast<jint>(methods.size()));
    if (regResult != 0) {
        qWarning() << "Failed to register native methods for NetworkHelper:" << regResult;
        env->DeleteLocalRef(clazz);
        return false;
    }

    qDebug() << "Registered native methods for NetworkHelper";
    env->DeleteLocalRef(clazz);

    return true;
}

bool registerNativeMethods_FileHelper(const QJniObject &m_javaHelper)
{
    const auto env = QJniEnvironment().jniEnv();
    if (!env) {
        return false;
    }

    // Get the class from the already-loaded object, not FindClass
    const auto clazz = env->GetObjectClass(m_javaHelper.object<jobject>());
    if (!clazz) {
        return false;
    }

    constexpr std::vector<JNINativeMethod> methods = {};
    const auto regResult = env->RegisterNatives(clazz, methods.data(), static_cast<jint>(methods.size()));
    if (regResult != 0) {
        qWarning() << "Failed to register native methods for FileHelper:" << regResult;
        env->DeleteLocalRef(clazz);
        return false;
    }

    qDebug() << "Registered native methods for FileHelper";
    env->DeleteLocalRef(clazz);

    return true;
}

bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper)
{
    if (m_javaHelper.isValid()) {
        return true;
    }

    const auto activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

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

    const auto activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

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

bool createNativeObject_FileHelper(Platform::FileProvider &provider)
{
    JavaProxy proxy(provider);
    auto &m_javaHelper = proxy.object();

    if (m_javaHelper.isValid()) {
        return true;
    }

    const auto activity = QJniObject::callStaticObjectMethod("org/qtproject/qt/android/QtNative", "activity", "()Landroid/app/Activity;");

    if (!activity.isValid()) {
        qWarning() << "Failed to get Android activity";
        return false;
    }

    m_javaHelper = QJniObject("eu/n1coc4cola/drming/FileHelper", "(Landroid/content/Context;)V", activity.object<jobject>());

    if (!m_javaHelper.isValid()) {
        qWarning() << "Failed to create FileHelper instance";
        return false;
    }

    // Register AFTER we have a live object, so GetObjectClass works
    if (!registerNativeMethods_FileHelper(m_javaHelper)) {
        qWarning() << "Failed to register native methods";
        return false;
    }

    proxy.setup();

    return true;
}
