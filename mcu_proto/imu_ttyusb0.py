

import math

if __name__ == '__main__':
    # filepath = 'ttyS3.bin'
    filepath = 'ttyUSB0.bin'
    file = open(filepath, 'rb')
    data = file.read()
    file.close()

    start_mark = int.from_bytes(b'\xa5', 'little')
    end_mark = int.from_bytes(b'\x5a', 'little')

    pos = 0
    count = 0
    pkt_len = 0
    cal_sum = 0

    while pos < len(data) - 30:
        if data[pos] == end_mark and data[pos + 1] == start_mark:
            count += 1
            
            pos += 2
            pkt_len = data[pos]

            sum_pos = pos + pkt_len + 1
            pkt_sum = int.from_bytes(data[sum_pos:sum_pos+2], 'little')

            cal_sum = 0
            for i in range(pkt_len + 1):
                val = data[pos + i]
                if (i > 0):
                    print(' {0:02x}'.format(val), end='')
                cal_sum += val
            
            print(' <{0}[{0:02x}], {1:04x}, {2:04x}>\n'.format(pkt_len, cal_sum, pkt_sum), end='')

            if (pkt_sum != cal_sum):
                print("   ****** {0:04x} != {1:04x} SKIP\n".format(pkt_sum, cal_sum), end='')
                continue

            if (data[pos + 1] != 1):
                print("   ****** unknown package SKIP")
                continue

            remap_data = data[pos:pos + 3] + b'\x00' + data[pos + 3:pos + 3 + 1] + b'\x00\x00\x00' + data[pos + 7:pos + 7 + 18]

            pkt_pos = 0#pos - 1 #+ 1

            s1 = int.from_bytes(remap_data[pkt_pos + 0x08:pkt_pos + 0x08 + 2], 'little', signed=False)
            s5 = int.from_bytes(remap_data[pkt_pos + 0x0a:pkt_pos + 0x0a + 2], 'little', signed=True)
            s7 = int.from_bytes(remap_data[pkt_pos + 0x0c:pkt_pos + 0x0c + 2], 'little', signed=True)
            s6 = int.from_bytes(remap_data[pkt_pos + 0x0e:pkt_pos + 0x0e + 2], 'little', signed=True)
            s1 = float(s1) / 1000.0 * 3.0517578125000000e-5 #0x400921fb54442d18ULL
            s5 = float(s5) * s1 # accel x
            s7 = float(s7) * s1 # accel y
            s6 = float(s6) * s1 # accel z

            s0 = int.from_bytes(remap_data[pkt_pos + 0x10:pkt_pos + 0x10 + 2], 'little', signed=False)
            s4 = int.from_bytes(remap_data[pkt_pos + 0x12:pkt_pos + 0x12 + 2], 'little', signed=True)
            s3 = int.from_bytes(remap_data[pkt_pos + 0x14:pkt_pos + 0x14 + 2], 'little', signed=True)
            s2 = int.from_bytes(remap_data[pkt_pos + 0x16:pkt_pos + 0x16 + 2], 'little', signed=True)
            s0 = float(s0) * math.pi / 180.0 * 3.0517578125000000e-5
            s4 = float(s4) * s0 # gyro x
            s3 = float(s3) * s0 # gyro y
            s2 = float(s2) * s0 # gyro z

            ss2 = int.from_bytes(remap_data[pkt_pos + 0x18:pkt_pos + 0x18 + 2], 'little', signed=True)
            ss2 = float(ss2) * 0.1 # temp

            print(' {0:6.6f} [ {1:6.6f} {2:6.6f} {3:6.6f} ], {4:6.6f} [ {5:6.6f} {6:6.6f} {7:6.6f} ], {8:2.1f}'.format(s1, s5, s7, s6, s0, s4, s3, s2, ss2))

        else:
            pos += 1
            cal_sum = 0

        if (count > 16):
            break

    print("count {count}".format(count = count))