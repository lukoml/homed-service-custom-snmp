FROM    127.0.0.1:5000/homed-service:latest
ARG     TARGETARCH
RUN     rm /var/lib/dpkg/info/libc-bin.*
RUN     sed -i '/bullseye-security/d' /etc/apt/sources.list
RUN     apt update
RUN     apt install -y snmp
RUN     mkdir /etc/homed
RUN     ln -s /data/homed-custom-snmp.conf /etc/homed/homed-custom-snmp.conf
RUN     ln -s /data/homed.log /var/log/homed.log
COPY    homed-custom-snmp-$TARGETARCH /usr/bin/homed-custom-snmp
CMD     ["/usr/bin/homed-custom-snmp"]
