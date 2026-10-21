// NOT REVIEWED

#ifndef DEVICE_H
#define DEVICE_H

#define REQUEST_TIMEOUT             1000
#define REQUEST_RETRIES             2
#define REQUEST_OID_LIMIT           10

#define SNMP_TRANSPORT_ERROR        1

#define OID_SYS_UPTIME              "1.3.6.1.2.1.1.3.0"
#define OID_IF_TYPE                 "1.3.6.1.2.1.2.2.1.3"
#define OID_IF_ADMIN_STATUS         "1.3.6.1.2.1.2.2.1.7"
#define OID_IF_OPER_STATUS          "1.3.6.1.2.1.2.2.1.8"
#define OID_IF_HIGH_SPEED           "1.3.6.1.2.1.31.1.1.1.15"
#define OID_IF_ALIAS                "1.3.6.1.2.1.31.1.1.1.18"
#define OID_HR_STORAGE_RAM          "1.3.6.1.2.1.25.2.1.2"
#define OID_HR_STORAGE_ENTRY        "1.3.6.1.2.1.25.2.3.1"
#define OID_HR_STORAGE_TYPE         "1.3.6.1.2.1.25.2.3.1.2"
#define OID_HR_STORAGE_DESCR        "1.3.6.1.2.1.25.2.3.1.3"
#define OID_HR_STORAGE_UNITS        "1.3.6.1.2.1.25.2.3.1.4"
#define OID_HR_STORAGE_SIZE         "1.3.6.1.2.1.25.2.3.1.5"
#define OID_HR_STORAGE_USED         "1.3.6.1.2.1.25.2.3.1.6"
#define OID_HR_PROCESSOR_LOAD       "1.3.6.1.2.1.25.3.3.1.2"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>

enum class Availability
{
    Unknown,
    Online,
    Offline
};

class DeviceObject;
typedef QSharedPointer <DeviceObject> Device;

class DeviceObject : public QObject
{
    Q_OBJECT

public:

    DeviceObject(const QString &address, const QString &community, const QString &ports, quint32 pollInterval, const QString &id, bool debug) :
        QObject(nullptr), m_id(id), m_debug(debug), m_address(address), m_community(community), m_ports(ports), m_pollInterval(pollInterval), m_pollTime(0), m_discovery(true), m_polling(false), m_ready(false), m_published(false), m_availability(Availability::Unknown), m_size(0) {}

    inline QString id(void) { return m_id; }
    inline QString name(void) { return m_data.value("name").toString(m_id); }

    inline bool ready(void) { return m_ready; }
    inline Availability availability(void) { return m_availability; }

    inline bool published(void) { return m_published; }
    inline void setPublished(bool value) { m_published = value; }

    inline QJsonArray exposes(void) { return m_exposes; }
    inline QJsonObject options(void) { return m_options; }

    inline QJsonObject data(void) { return m_data; }
    inline void setData(const QJsonObject &value) { m_data = value; }

    inline QMap <QString, QVariant> properties(void) { return m_properties; }

    inline quint32 pollInterval(void) { return m_pollInterval; }
    inline qint64 pollTime(void) { return m_pollTime; }
    inline bool polling(void) { return m_polling; }

    void poll(void);

private:

    QString m_id;
    bool m_debug;

    QString m_address, m_community, m_ports;
    quint32 m_pollInterval;
    qint64 m_pollTime;
    bool m_discovery, m_polling, m_ready, m_published;

    Availability m_availability;

    QJsonArray m_exposes;
    QJsonObject m_options, m_data;

    QList <QString> m_oids, m_cpu, m_buffers;
    QMap <QString, QString> m_items;

    QString m_ram;
    QMap <QString, qint64> m_units;
    qint64 m_size;

    QList <QList <QString>> m_queue;
    QMap <QString, QString> m_values;
    QMap <QString, QVariant> m_properties;

    void updateAvailability(Availability availability);
    void sendRequest(void);

    void parseDiscovery(void);
    void parseProperties(void);

private slots:

    void finished(int exitCode, QProcess::ExitStatus exitStatus);

signals:

    void deviceUpdated(void);
    void availabilityUpdated(void);
    void propertiesUpdated(void);

};

inline QDebug operator << (QDebug debug, DeviceObject *device) { return debug << "device" << device->id(); }

#endif
