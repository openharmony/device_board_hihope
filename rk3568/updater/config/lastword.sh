mkdir /backdata
mount /dev/block/by-name/userdata /backdata

if [ -d "/backdata/updater/log/faultlog" ];
then
    rm -rf /backdata/updater/log/faultlog/*
    cp /data/log/faultlog/temp/cpp* /backdata/updater/log/faultlog
else
    mkdir /backdata/updater/log/faultlog
    cp /data/log/faultlog/temp/cpp* /backdata/updater/log/faultlog
fi

if [ -d "/backdata/updater/log/hilog" ];
then
    rm -rf /backdata/updater/log/hilog/*
    cp /data/log/hilog/updater* /backdata/updater/log/hilog
else
    mkdir /backdata/updater/log/hilog
    cp /data/log/hilog/updater* /backdata/updater/log/hilog
fi
