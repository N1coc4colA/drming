#ifndef NATIVE_H
#define NATIVE_H

class QJniObject;

namespace Platform {

class FileProvider;

}

bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper);
bool createNativeObject_NetworkHelper(QJniObject &m_javaHelper);
bool createNativeObject_FileHelper(Platform::FileProvider &provider);

#endif // NATIVE_H
