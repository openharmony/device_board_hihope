mkdir /backdata
mount /dev/block/by-name/userdata /backdata
rm -rf /backdata/updater/log/*.gz
cp /data/log/hilog/updater* /backdata/updater/log
cp /data/log/faultlog/temp/cpp* /backdata/updater/log