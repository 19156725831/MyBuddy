/****************************************************************************
** Meta object code from reading C++ file 'aiclient.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/aiclient.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'aiclient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8AIClientE_t {};
} // unnamed namespace

template <> constexpr inline auto AIClient::qt_create_metaobjectdata<qt_meta_tag_ZN8AIClientE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "AIClient",
        "messageReceived",
        "",
        "QJsonObject",
        "message",
        "promptTokens",
        "completionTokens",
        "requestFailed",
        "requestFinished",
        "searchCompleted",
        "QJsonArray",
        "results",
        "searchFailed",
        "balanceReceived",
        "balance",
        "embeddingReceived",
        "requestId",
        "embedding",
        "embeddingFailed",
        "imageDescriptionReceived",
        "description",
        "imageDescriptionFailed"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'messageReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &, int, int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { QMetaType::Int, 5 }, { QMetaType::Int, 6 },
        }}),
        // Signal 'requestFailed'
        QtMocHelpers::SignalData<void(const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Signal 'requestFinished'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'searchCompleted'
        QtMocHelpers::SignalData<void(const QJsonArray &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'searchFailed'
        QtMocHelpers::SignalData<void(const QString &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 4 },
        }}),
        // Signal 'balanceReceived'
        QtMocHelpers::SignalData<void(const QString &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 14 },
        }}),
        // Signal 'embeddingReceived'
        QtMocHelpers::SignalData<void(quint64, const QJsonArray &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 16 }, { 0x80000000 | 10, 17 },
        }}),
        // Signal 'embeddingFailed'
        QtMocHelpers::SignalData<void(quint64, const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 16 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'imageDescriptionReceived'
        QtMocHelpers::SignalData<void(quint64, const QString &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 16 }, { QMetaType::QString, 20 },
        }}),
        // Signal 'imageDescriptionFailed'
        QtMocHelpers::SignalData<void(quint64, const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::ULongLong, 16 }, { QMetaType::QString, 4 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<AIClient, qt_meta_tag_ZN8AIClientE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject AIClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AIClientE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AIClientE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8AIClientE_t>.metaTypes,
    nullptr
} };

void AIClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<AIClient *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->messageReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[3]))); break;
        case 1: _t->requestFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->requestFinished(); break;
        case 3: _t->searchCompleted((*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[1]))); break;
        case 4: _t->searchFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->balanceReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->embeddingReceived((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[2]))); break;
        case 7: _t->embeddingFailed((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 8: _t->imageDescriptionReceived((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 9: _t->imageDescriptionFailed((*reinterpret_cast<std::add_pointer_t<quint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(const QJsonObject & , int , int )>(_a, &AIClient::messageReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(const QString & )>(_a, &AIClient::requestFailed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)()>(_a, &AIClient::requestFinished, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(const QJsonArray & )>(_a, &AIClient::searchCompleted, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(const QString & )>(_a, &AIClient::searchFailed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(const QString & )>(_a, &AIClient::balanceReceived, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(quint64 , const QJsonArray & )>(_a, &AIClient::embeddingReceived, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(quint64 , const QString & )>(_a, &AIClient::embeddingFailed, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(quint64 , const QString & )>(_a, &AIClient::imageDescriptionReceived, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (AIClient::*)(quint64 , const QString & )>(_a, &AIClient::imageDescriptionFailed, 9))
            return;
    }
}

const QMetaObject *AIClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AIClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8AIClientE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int AIClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void AIClient::messageReceived(const QJsonObject & _t1, int _t2, int _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3);
}

// SIGNAL 1
void AIClient::requestFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void AIClient::requestFinished()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void AIClient::searchCompleted(const QJsonArray & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void AIClient::searchFailed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void AIClient::balanceReceived(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void AIClient::embeddingReceived(quint64 _t1, const QJsonArray & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void AIClient::embeddingFailed(quint64 _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1, _t2);
}

// SIGNAL 8
void AIClient::imageDescriptionReceived(quint64 _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}

// SIGNAL 9
void AIClient::imageDescriptionFailed(quint64 _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}
QT_WARNING_POP
