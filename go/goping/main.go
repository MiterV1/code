package main

import (
	"bytes"
	"container/list"
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

func showStatistics(host string, sent int, l *list.List) {
	fmt.Println("")
	//信息统计

	var min, max, sum int
	if l.Len() == 0 {
		min, max, sum = 0, 0, 0
	} else {
		min, max, sum = l.Front().Value.(int), l.Front().Value.(int), 0
	}

	/* 链表遍历 */
	for v := l.Front(); v != nil; v = v.Next() {
		val := v.Value.(int)
		switch {
		case val < min:
			min = val
		case val > max:
			max = val
		}
		sum += val
	}

	recv, lost := l.Len(), sent-l.Len()
	fmt.Printf("%s 的 Ping 统计信息：\n", host)
	fmt.Printf("数据包：已发送 = %d，已接收 = %d，丢失 = %d (%.1f%% 丢失)\n", sent, recv, lost, float32(lost)/float32(sent)*100)
	fmt.Printf("往返行程的估计时间(以毫秒为单位)：\n")
	fmt.Printf("最短 = %dms，最长 = %dms，平均 = %.0fms\n", min, max, float32(sum)/float32(recv))
}

func ping(domain string, count int) {
	sip := net.IPAddr{IP: net.ParseIP("0.0.0.0")}
	dip, err := net.ResolveIPAddr("ip", domain)
	if err != nil {
		fmt.Println(err.Error())
		return
	}

	/* 构造icmp包 */
	fmt.Printf("正在 Ping %s [%s] 具有 %d 字节的数据:\n", domain, dip.String(), 32)
	conn, err := net.DialIP("ip4:icmp", &sip, dip)
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

	sent := 0
	recv := make([]byte, 1500)
	statistic := list.New() /* 使用链表存储每次的duration值 */
	for i := 0; i < count; i++ {
		var err error

		/* 发送icmp数据包 */
		_, err = conn.Write(b)
		if err != nil {
			time.Sleep(time.Second)
			continue
		}
		sent++

		tStart := time.Now()
		conn.SetReadDeadline((time.Now().Add(time.Second * 5))) // 5s内未接收到，则认为超时
		_, err = conn.Read(recv)
		if err != nil {
			fmt.Println("请求超时")
			continue
		}
		tEnd := time.Now()

		tDuration := int(tEnd.Sub(tStart).Nanoseconds() / 1e6) // 毫秒计算
		fmt.Printf("来自 %s 的回复: 字节=%d 时间 = %dms TTL=%d\n", dip.String(), 32, tDuration, 64)
		statistic.PushBack(tDuration)

		time.Sleep(time.Second)
	}

	showStatistics(dip.String(), sent, statistic)
}

func usage() {
	fmt.Printf("./a.out domain | times\n")
	fmt.Printf("Example: ./a.out www.google.com 2\n")
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
