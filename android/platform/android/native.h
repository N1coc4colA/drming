#ifndef NATIVE_H
#define NATIVE_H

class QJniObject;

bool createNativeObject_MdnsHelper(QJniObject &m_javaHelper);
bool createNativeObject_NetworkHelper(QJniObject &m_javaHelper);

#endif // NATIVE_H
