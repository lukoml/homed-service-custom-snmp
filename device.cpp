// NOT REVIEWED

#include <math.h>
#include "device.h"
#include "logger.h"

void DeviceObject::poll(void)
{
    m_pollTime = QDateTime::currentMSecsSinceEpoch();
    m_polling = true;
    m_queue.clear();
    m_values.clear();

    if (m_discovery)
    {
        if (m_ports == "all")
            m_queue.append(QList <QString> {OID_IF_TYPE});

        if (!m_ports.isEmpty())
            m_queue.append(QList <QString> {OID_IF_ALIAS});

        m_queue.append(QList <QString> {OID_HR_PROCESSOR_LOAD});
        m_queue.append(QList <QString> {OID_HR_STORAGE_ENTRY});
    }
    else
    {
        for (int i = 0; i < m_oids.count(); i += REQUEST_OID_LIMIT)
            m_queue.append(m_oids.mid(i, REQUEST_OID_LIMIT));
    }

    sendRequest();
}

void DeviceObject::updateAvailability(Availability availability)
{
    if (m_availability == availability)
        return;

    m_availability = availability;
    emit availabilityUpdated();
}

void DeviceObject::sendRequest(void)
{
    QProcess *process(new QProcess(this));
    QString program = m_discovery ? "snmpwalk" : "snmpget";
    QList <QString> oids = m_queue.takeFirst(), arguments = {"-v2c", "-c", m_community, "-t", QString::number(REQUEST_TIMEOUT / 1000), "-r", QString::number(REQUEST_RETRIES), "-m", QString(), "-On", "-Oq", "-Oe", "-Ot", m_address};

    connect(process, static_cast <void (QProcess::*)(int, QProcess::ExitStatus)> (&QProcess::finished), this, &DeviceObject::finished);

    logDebug(m_debug) << this << program.toUtf8().constData() << "request:" << oids.join(", ").toUtf8().constData();
    process->start(program, arguments + oids);
}

void DeviceObject::parseDiscovery(void)
{
    QList <QString> names = {"name", "enabled", "link", "speed"}, oids = {OID_IF_ALIAS, OID_IF_ADMIN_STATUS, OID_IF_OPER_STATUS, OID_IF_HIGH_SPEED};
    QList <int> ports;
    QJsonArray exposes;
    QJsonObject options;

    m_oids.clear();
    m_items.clear();
    m_cpu.clear();
    m_buffers.clear();
    m_ram.clear();
    m_units.clear();
    m_size = 0;

    for (auto it = m_values.begin(); it != m_values.end(); it++)
    {
        QString index = it.key().split('.').last();

        if (it.key().startsWith(QString("%1.").arg(OID_IF_TYPE)) && it.value() == "6") // ethernetCsmacd
        {
            ports.append(index.toInt());
        }
        else if (it.key().startsWith(QString("%1.").arg(OID_HR_PROCESSOR_LOAD)))
        {
            m_cpu.append(it.key());
        }
        else if (it.key().startsWith(QString("%1.").arg(OID_HR_STORAGE_TYPE)))
        {
            QString description = m_values.value(QString("%1.%2").arg(OID_HR_STORAGE_DESCR, index));

            if (it.value() == QString(".%1").arg(OID_HR_STORAGE_RAM))
                m_ram = index;
            else if (description == "Memory buffers" || description == "Cached memory")
                m_buffers.append(index);
            else
                continue;

            m_units.insert(index, m_values.value(QString("%1.%2").arg(OID_HR_STORAGE_UNITS, index)).toLongLong());
        }
    }

    if (m_ports != "all")
    {
        QList <QString> list = m_ports.split(',', Qt::SkipEmptyParts);

        for (int i = 0; i < list.count(); i++)
        {
            QList <QString> range = list.at(i).trimmed().split('-');

            for (int j = range.value(0).toInt(); j <= range.value(1, range.value(0)).toInt(); j++)
                ports.append(j);
        }
    }

    std::sort(ports.begin(), ports.end());

    if (!ports.isEmpty())
    {
        options.insert("name", QJsonObject {{"type", "sensor"}, {"icon", "mdi:tag-outline"}});
        options.insert("enabled", QJsonObject {{"type", "binary"}, {"icon", "mdi:toggle-switch-outline"}});
        options.insert("link", QJsonObject {{"type", "binary"}, {"icon", "mdi:ethernet"}});
        options.insert("speed", QJsonObject {{"type", "sensor"}, {"unit", "Mbit/s"}});
    }

    for (int i = 0; i < ports.count(); i++)
    {
        for (int j = 0; j < names.count(); j++)
        {
            QString name = ports.count() > 1 ? QString("%1_%2").arg(names.at(j)).arg(ports.at(i)) : names.at(j), oid = QString("%1.%2").arg(oids.at(j)).arg(ports.at(i));

            if (oids.at(j) == OID_IF_ALIAS && m_values.value(oid).isEmpty())
                continue;

            exposes.append(name);
            m_oids.append(oid);
            m_items.insert(oid, name);
        }
    }

    if (!m_cpu.isEmpty())
    {
        exposes.append("cpu");
        options.insert("cpu", QJsonObject {{"type", "sensor"}, {"unit", "%"}, {"icon", "mdi:cpu-64-bit"}});
        m_oids.append(m_cpu);
    }

    if (!m_ram.isEmpty())
    {
        m_size = m_values.value(QString("%1.%2").arg(OID_HR_STORAGE_SIZE, m_ram)).toLongLong() * m_units.value(m_ram);
        exposes.append("memory");
        options.insert("memory", QJsonObject {{"type", "sensor"}, {"unit", "%"}, {"icon", "mdi:memory"}});
        m_oids.append(QString("%1.%2").arg(OID_HR_STORAGE_USED, m_ram));

        for (int i = 0; i < m_buffers.count(); i++)
            m_oids.append(QString("%1.%2").arg(OID_HR_STORAGE_USED, m_buffers.at(i)));
    }

    exposes.append("uptime");
    options.insert("uptime", QJsonObject {{"type", "sensor"}, {"unit", "sec"}, {"icon", "mdi:timer-outline"}});
    m_oids.append(OID_SYS_UPTIME);
    m_items.insert(OID_SYS_UPTIME, "uptime");

    m_discovery = false;
    m_pollTime = 0;

    if (m_ready && m_exposes == exposes && m_options == options)
        return;

    m_exposes = exposes;
    m_options = options;
    m_ready = true;
    m_published = false;

    emit deviceUpdated();
}

void DeviceObject::parseProperties(void)
{
    QList <QString> list = {"uptime", "name", "enabled", "link", "speed"};
    QMap <QString, QVariant> properties;

    for (auto it = m_items.begin(); it != m_items.end(); it++)
    {
        QString value = m_values.value(it.key());

        if (!m_values.contains(it.key()))
            continue;

        switch (list.indexOf(it.value().split('_').value(0)))
        {
            case 0: properties.insert(it.value(), value.toLongLong() / 100); break; // uptime
            case 1: properties.insert(it.value(), value); break; // name
            case 2 ... 3: properties.insert(it.value(), value.toInt() == 1); break; // enabled, link
            case 4: properties.insert(it.value(), value.toInt()); break; // speed
        }
    }

    if (!m_cpu.isEmpty())
    {
        double value = 0;

        for (int i = 0; i < m_cpu.count(); i++)
            value += m_values.value(m_cpu.at(i)).toDouble();

        properties.insert("cpu", round(value * 10 / m_cpu.count()) / 10);
    }

    if (m_size)
    {
        qint64 value = m_values.value(QString("%1.%2").arg(OID_HR_STORAGE_USED, m_ram)).toLongLong() * m_units.value(m_ram);

        for (int i = 0; i < m_buffers.count(); i++)
            value -= m_values.value(QString("%1.%2").arg(OID_HR_STORAGE_USED, m_buffers.at(i))).toLongLong() * m_units.value(m_buffers.at(i));

        properties.insert("memory", round(value * 1000.0 / m_size) / 10);
    }

    if (m_properties == properties)
        return;

    m_properties = properties;
    emit propertiesUpdated();
}

void DeviceObject::finished(int exitCode, QProcess::ExitStatus)
{
    QProcess *process = reinterpret_cast <QProcess*> (sender());
    QByteArray output = process->readAllStandardOutput(), error = process->readAllStandardError();
    QList <QString> list = QString(output).split('\n', Qt::SkipEmptyParts);

    logDebug(m_debug) << this << "reply:" << (exitCode ? error : output).trimmed().constData();
    process->deleteLater();

    if (exitCode == SNMP_TRANSPORT_ERROR)
    {
        m_discovery = true;
        m_polling = false;
        updateAvailability(Availability::Offline);
        return;
    }

    if (exitCode)
        logWarning << this << "request error:" << error.trimmed().replace('\n', 0x20).constData();

    for (int i = 0; i < list.count(); i++)
    {
        QString item = list.at(i), value;
        int index = item.indexOf(0x20);

        if (!item.startsWith('.') || index < 0)
            continue;

        value = item.mid(index + 1);

        if (value.startsWith('"') && value.endsWith('"'))
            value = value.mid(1, value.length() - 2);
        else if (value.contains(0x20))
            continue;

        m_values.insert(item.mid(1, index - 1), value);
    }

    if (!m_queue.isEmpty())
    {
        sendRequest();
        return;
    }

    m_polling = false;
    updateAvailability(Availability::Online);

    if (m_discovery)
    {
        parseDiscovery();
        return;
    }

    parseProperties();
}
