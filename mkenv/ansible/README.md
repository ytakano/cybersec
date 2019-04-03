

## Vagrant + libvirtのインストール

setup.ymlファイル中のuseridと、hostsファイル中のホストを適宜変更すること。

```
$ ansible-playbook -i hosts setup.yml
```

## サーバがUbuntu 18.04の場合

デフォルトでpython3しかインストールされていないため、
```-e 'ansible_python_interpreter=/usr/bin/python3'```
とオプションに指定する必要あり。

例
```
$ ansible-playbook -i hosts setup.yml -e 'ansible_python_interpreter=/usr/bin/python3'
```

## サーバがUbuntu 18.10の場合

```vagrant plugin install vagrant-libvirt```
がうまくいかないので諦めた。

## sudoのパスワード

sudoにパスワードが必要な場合は、
```--ask-become-pass```
とオプションを指定する必要あり。

例
```
$ ansible-playbook -i hosts --ask-become-pass setup.yml
```