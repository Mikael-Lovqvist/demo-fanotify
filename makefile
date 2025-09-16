demo: build/watch-fs demo/mount/lost+found
	sudo ./build/watch-fs ./demo/mount; \
	sudo umount demo/mount

demo/mount/lost+found:
	mkdir -p demo/mount
	rm -f demo/fs.img
	truncate -s64M demo/fs.img
	mkfs.ext4 demo/fs.img
	sudo mount demo/fs.img demo/mount
	sudo chown `id -nu`:`id -ng` demo/mount/

clean:
	@if mountpoint -q demo/mount; then \
		sudo umount demo/mount; \
	fi
	rm -rf demo build

build/watch-fs: watch-fs.c
	mkdir -p build;
	gcc $< -o $@

.PHONY: demo setup clean