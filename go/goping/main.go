package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net"
	"os"
	"strconv"
	"time"
)

type imcpProto struct {
	typeProto   uint8
	code        uint8
	checksum    uint16
	identifier  uint16
	sequenceNum uint16
}

func checkSum(data []byte) (rt uint16) {
	var (
		sum    uint32
		length int = len(data)
		index  int
	)

	for length > 1 {
		sum += uint32(data[index])<<8 + uint32(data[index+1])
		index += 2
		length -= 2
	}

	if length > 0 {
		sum += uint32(data[index]) << 8
	}
	rt = uint16(sum) + uint16(sum>>16)

	return ^rt
}

func ping(domain string, count int) {
	laddr := net.IPAddr{IP: net.ParseIP("0.0.0.0")}
	raddr, _ := net.ResolveIPAddr("ip", domain)

	/* 构造icmp包 */
	conn, err := net.DialIP("ip4:icmp", &laddr, raddr)
	if err != nil {
		fmt.Println(err.Error())
		return
	}
	defer conn.Close()

	var buffer bytes.Buffer
	var icmp = imcpProto{8, 0, 0, 0, 0} /* request for icmp */
	binary.Write(&buffer, binary.BigEndian, icmp)
	b := buffer.Bytes()
	binary.BigEndian.PutUint16(b[2:], checkSum(b))

	recv := make([]byte, 1500)
	for i := 0; i < count; i++ {
		var err error

		/* 发送icmp数据包 */
		_, err = conn.Write(b)
		if err != nil {
			time.Sleep(time.Second)
			continue
		}

		tStart := time.Now()
		conn.SetReadDeadline((time.Now().Add(time.Second * 5))) // 5s内未接收到，则认为超时
		_, err = conn.Read(recv)
		if err != nil {
			fmt.Println("请求超时")
			continue
		}
		tEnd := time.Now()

		tDuration := tEnd.Sub(tStart).Nanoseconds() / 1e6 // 毫秒计算
		fmt.Printf("来自 %s 的回复: 时间 = %dms\n", raddr.String(), tDuration)

		time.Sleep(time.Second)
	}
}

func usage() {
	fmt.Printf("./a.out domain | times\n")
	fmt.Printf("Example: ./a.out www.google.com 4\n")
}

func main() {
	if len(os.Args) < 2 {
		usage()
		os.Exit(1)
	}

	count, err := strconv.Atoi(os.Args[2])
	if err != nil {
		fmt.Println("please input correct counts")
		os.Exit(1)
	}

	ping(os.Args[1], count)
}
