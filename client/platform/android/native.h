#ifndef NATIVE_H
#define NATIVE_H

class QJniObject;

namespace Platform {

class FileProvider;

}

/// Creates the Java mDNS helper object.
bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper);
/// Creates the Java network helper object.
bool createNativeObject_NetworkHelper(QJniObject &m_javaHelper);
/// Creates the Java file helper object.
bool createNativeObject_FileHelper(Platform::FileProvider &provider);
/// Creates the Java multicast-lock helper object.
bool createNativeObject_MulticastLockHelper(QJniObject &m_javaHelper);
/// Creates the Java Android UI helper object.
bool createNativeObject_AndroidUi(QJniObject &m_javaHelper);
/// Registers the activity lifecycle callback.
bool registerNativeMethods_DrmingActivity();

#endif // NATIVE_H
