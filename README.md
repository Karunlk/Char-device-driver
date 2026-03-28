#  Character Device Driver
**Author:** K. D. Karun Lakshman

---

## Prerequisites

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

---

## Build

```bash
make
```

This produces `b240213cs_A1.ko` in the current directory.

To clean build artifacts:
```bash
make clean
```

---

## Execution Steps
### Note : Disable module signature enforcement or Sign the module yourself or Disable Secure Boot in BIOS before starting step 1.
### Step 1 — Find your kernel version
```bash
uname -r
# e.g. 6.5.0-45-generic  →  use kernel_version=6,5
```

### Step 2 — Insert the module
```bash
sudo insmod b240213cs_A1.ko kernel_version=6,5 timer=30
```
Replace `6,5` with your actual major.minor version.
Replace `30` with however many seconds you want.

### Step 3 — Check dmesg for the major number
```bash
dmesg | tail -10
# Look for: mydev: registered — major=<N> minor=0 timer=30
```

### Step 4 — Create the device node
```bash
sudo mknod /dev/mydev c <major_number> 0
sudo chmod 666 /dev/mydev
```
Replace `<major_number>` with the number shown in dmesg.

### Step 5 — Perform the read action
```bash
cat /dev/mydev
```
This triggers `read_done = 1` inside the driver.

### Step 6 — Perform the write action (use your actual username)
```bash
echo $(whoami) > /dev/mydev
```
This triggers `write_done = 1` and wakes the exit waitqueue.

### Step 7 — Remove the module
```bash
sudo rmmod b240213cs_A1
```

### Step 8 — Check the verdict
```bash
dmesg | tail -10
# Success: "Successfully completed the actions within time. Username: <name>"
# Failure: appropriate failure message
```

---

## Testing the Failure Case

Insert module, wait for the timer to expire without doing cat/echo, then rmmod:
```bash
sudo insmod b240213cs_A1.ko kernel_version=6,5 timer=10
# wait 10 seconds
sudo rmmod b240213cs_A1
dmesg | tail -5
```

---

## Testing Version Mismatch

```bash
sudo insmod b240213cs_A1.ko kernel_version=5,2 timer=30
# insmod will fail — check dmesg for version mismatch error
dmesg | tail -3
```

---

## Cleanup

```bash
sudo rm -f /dev/mydev
make clean
```

---

## File Structure

```
Char-device-driver/
├── b240213cs_A1.c     ← driver source code
├── Makefile           ← builds the .ko kernel module
├── README.md          ← this file
└── test.sh            ← a bash script to test the device driver
 ```
