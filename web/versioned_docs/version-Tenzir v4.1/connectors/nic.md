# nic

Reads bytes from a network interface card (NIC).

[pcap-rfc]: https://datatracker.ietf.org/doc/id/draft-gharris-opsawg-pcap-00.html

## Synopsis

```
nic <iface> [-s|--snaplen <count>]
```

## Description

The `nic` loader uses libpcap to acquire packets from a network interface and
packs them into blocks of bytes that represent PCAP packet records.

The received first packet triggers also emission of PCAP file header such that
downstream operators can treat the packet stream as valid PCAP capture file.

The default parser for the `nic` loader is [`pcap`](../formats/pcap.md).

### `-s|--snaplen <count>`

Sets the snapshot length of the captured packets.

This value is an upper bound on the packet size. Packets larger than this size
get truncated to `<count>` bytes.

Defaults to `262144`.

## Examples

Read PCAP packets from `eth0`:

```
from nic eth0
```

Perform the equivalent of `tcpdump -i en0 -w trace.pcap`:

```
load nic en0 | save file trace.pcap
```
