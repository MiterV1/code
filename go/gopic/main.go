package main

import (
	"container/list"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"strconv"

	"github.com/rwcarlsen/goexif/exif"
)

func usage() {
	fmt.Println("usage:")
	fmt.Println("a.out directory")
}

func decodeExif(file string) (int, int, error) {
	f, err := os.Open(file)
	if err != nil {
		return 0, 0, err
	}
	defer f.Close()

	x, err := exif.Decode(f)
	if err != nil {
		return 0, 0, err
	}

	tm, err := x.DateTime()
	if err != nil {
		return 0, 0, err
	}

	return tm.Year(), int(tm.Month()), nil
}

func readAllFile(dir string, hmap map[int]*list.List) (err error) {
	fileInfo, err := ioutil.ReadDir(dir)
	if err != nil {
		return errors.New("Open directory failed!")
	}

	for _, file := range fileInfo {
		absDir := path.Join(dir, file.Name())
		if file.IsDir() {
			err = readAllFile(absDir, hmap)
			if err != nil {
				return err
			}
		}

		if path.Ext(file.Name()) == ".jpg" {
			year, month, _ := decodeExif(absDir)
			l, ok := hmap[(year<<4)+month]
			if !ok {
				l = list.New()
				hmap[(year<<4)+month] = l
			}
			l.PushBack(absDir)
		}
	}

	return nil
}

func main() {
	if len(os.Args) < 2 {
		usage()
		return
	}

	dir := os.Args[1]
	src := make(map[int]*list.List, 1024)

	err := readAllFile(dir, src)
	if err != nil {
		fmt.Println(err)
	}

	for k, v := range src {
		year := k >> 4
		month := k % 12

		dstDir := fmt.Sprintf("./%s/%02s/", strconv.Itoa(year), strconv.Itoa(month))
		os.MkdirAll(dstDir, 0755)

		for i := v.Front(); i != nil; i = i.Next() {
			oriPath := i.Value.(string)
			dstPath := dstDir + path.Base(i.Value.(string))
			fmt.Printf("orig path=%s, dst path=%s\n", oriPath, dstPath)
			os.Rename(oriPath, dstPath)
		}
	}
}
