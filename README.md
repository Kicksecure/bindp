<<<<<<< HEAD
# Binding specific IP and Port for Linux Running Application #
=======
bindp
=====
>>>>>>> yongboy/master

## Intro

With LD\_PRELOAD and `bindp`, you can do:

<<<<<<< HEAD
## How to install `bindp` using apt-get ##

1\. Download the APT Signing Key.

```
wget https://www.kicksecure.com/keys/derivative.asc
```

Users can [check the Signing Key](https://www.kicksecure.com/wiki/Signing_Key) for better security.

2\. Add the APT Signing Key.

```
sudo cp ~/derivative.asc /usr/share/keyrings/derivative.asc
```

3\. Add the derivative repository.

```
echo "deb [signed-by=/usr/share/keyrings/derivative.asc] https://deb.kicksecure.com bookworm main contrib non-free" | sudo tee /etc/apt/sources.list.d/derivative.list
```

4\. Update your package lists.

```
sudo apt-get update
```

5\. Install `bindp`.

```
sudo apt-get install bindp
```

## How to Build deb Package from Source Code ##

Can be build using standard Debian package build tools such as:

```
dpkg-buildpackage -b
```

See instructions.

NOTE: Replace `generic-package` with the actual name of this package `bindp`.

* **A)** [easy](https://www.kicksecure.com/wiki/Dev/Build_Documentation/generic-package/easy), _OR_
* **B)** [including verifying software signatures](https://www.kicksecure.com/wiki/Dev/Build_Documentation/generic-package)

## Contact ##

* [Free Forum Support](https://forums.kicksecure.com)
* [Premium Support](https://www.kicksecure.com/wiki/Premium_Support)

## Donate ##

`bindp` requires [donations](https://www.kicksecure.com/wiki/Donate) to stay alive!
=======
- For server application
    - Assign ip and port for listening
    - Add SO_REUSEADDR/SO_REUSEPORT for existing application
- For socket client
    - Assign special ip and port for connection
    - Add SO_REUSEPORT for reuse the ip add port

## Compile

Compile on Linux with:

    make

## Usage

### IP & Port

How to use it:

    REUSE_ADDR=1 REUSE_PORT=1 BIND_ADDR="your ip" BIND_PORT="your port" LD_PRELOAD=/your_path/libindp.so The_Command_Here ...


Example in bash to make inetd only listen to the localhost
lo interface, thus disabling remote connections and only
enable to/from localhost:

    BIND_ADDR="127.0.0.1" BIND_PORT="49888" LD_PRELOAD=/your_path/libindp.so curl http://192.168.190.128

OR:

    BIND_ADDR="127.0.0.1" LD_PRELOAD=/your_path/libindp.so curl http://192.168.190.128

Just want to change the nginx's listen port:

    BIND_PORT=8888 LD_PRELOAD=/your_path/libindp.so /usr/sbin/nginx -c /etc/nginx/nginx.conf

Example in bash to use your virtual IP as your outgoing
sourceaddress for ircII:

    BIND_ADDR="your-virt-ip" LD_PRELOAD=/your_path/bind.so ircII

Note that you have to set up your server's virtual IP first.

### `SO_REUSEADDR`/`SO_REUSEPORT`

Now, I add the `SO_REUSEADDR`/`SO_REUSEPORT` support within Centos7 or Linux OS with kernel >= 3.9, for the old applictions with multi-process just listen the same port now:

    REUSE_ADDR=1 REUSE_PORT=1 LD_PRELOAD=./libindp.so python server.py &

OR

    REUSE_ADDR=1 REUSE_PORT=1 BIND_PORT=9999 LD_PRELOAD=./libindp.so java -server -jar your.jar &

With libindp.so's support, you can run your app multi-instance just for you need.

And, for socket client's connect you can also reuse the same client's ip and port:

    REUSE_PORT=1 BIND_ADDR="10.10.10.10" BIND_PORT=49999 LD_PRELOAD=/the_path/libindp.so nc 10.10.10.11 10001

    REUSE_PORT=1 BIND_ADDR="10.10.10.10" BIND_PORT=49999 LD_PRELOAD=/the_path/libindp.so nc 10.10.10.11 10011

### `IP_TRANSPARENT` Support

With `IP_TRANSPARENT` support, we can bind more nonlocal IP address, some one called "AnyIP".

```bash
IP_TRANSPARENT=1 REUSE_PORT=1 BIND_ADDR="NONLOCAL_IP" BIND_PORT=Your_Bind_Port LD_PRELOAD=/the_path/libindp.so nc The_Target_Address The_Target_Port
```

> The `IP_TRANSPARENT` operation need root right

And don't forget, you should have set the ip roule before, as below example:

```bash
ip rule add iif eth0 tab 100
ip route add local 0.0.0.0/0 dev lo tab 100
```

Enjoy it :))
>>>>>>> yongboy/master
