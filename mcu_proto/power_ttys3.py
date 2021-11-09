

if __name__ == '__main__':
    filepath = 'ttyS3.bin'
    # filepath = 'ttyUSB0.bin'
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
            
            print(' <{0}[{0:02x}], {1:04x}, {2:04x}> '.format(pkt_len, cal_sum, pkt_sum), end='')

            if (pkt_sum != cal_sum):
                print("   ****** {0:04x} != {1:04x} SKIP\n".format(pkt_sum, cal_sum), end='')
                continue

            pkt_pos = pos

            # w0 is length
            w1 = int.from_bytes(data[pkt_pos + 0x01:pkt_pos + 0x01 + 1], 'little', signed=False) # unknown/status
            w23 = int.from_bytes(data[pkt_pos + 0x02:pkt_pos + 0x02 + 2], 'little', signed=True) # volt
            w45 = int.from_bytes(data[pkt_pos + 0x04:pkt_pos + 0x04 + 2], 'little', signed=True) # curr
            w67 = int.from_bytes(data[pkt_pos + 0x06:pkt_pos + 0x06 + 2], 'little', signed=True) # temp
            w8 = int.from_bytes(data[pkt_pos + 0x08:pkt_pos + 0x08 + 1], 'little', signed=False) # parcent
            w9 = int.from_bytes(data[pkt_pos + 0x09:pkt_pos + 0x09 + 1], 'little', signed=False) # unknown/power_supply
            wa = int.from_bytes(data[pkt_pos + 0x0a:pkt_pos + 0x0a + 1], 'little', signed=False) # unknown/pwrboard_status
            wb = int.from_bytes(data[pkt_pos + 0x0b:pkt_pos + 0x0b + 1], 'little', signed=False) # health
            wcd = int.from_bytes(data[pkt_pos + 0x0c:pkt_pos + 0x0c + 2], 'little', signed=True) # cycle
            # wef is sum

            print("[ status:{0:2d} volt:{1} curr:{2:4d} temp:{3} vol:{4} power_supply:{5:2d} s2:{6} health:{7} cycle:{8:3d} ]".format(w1, w23, w45 * 10, w67 / 10, w8, w9, wa, wb, wcd))

        else:
            pos += 1
            cal_sum = 0

    print("count {count}".format(count = count))