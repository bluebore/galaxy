#!/bin/bash
BUILD_HOME=`pwd`
PROD=$BUILD_HOME/output/
CONFIG=$BUILD_HOME/production
echo "---------- start to build ---------"
test -e $PROD && rm -rf $PROD
mkdir -p $PROD
cd $BUILD_HOME/backend && sh compile-proto.sh
cp -rf $BUILD_HOME/backend/src/* $RPOD
# rm the soft link
rm $PROD/statics && mkdir $PROD/statics
cd $BUILD_HOME/ui && grunt build 
cp -rf $BUILD_HOME/ui/bower_components/todc-bootstrap/img $BUILD_HOME/ui/dist/ 
cp -rf $BUILD_HOME/ui/dist/* $PROD/statics

# replace config
cp -rf $CONFIG/settings.py $PROD/bootstrap/
cp $CONFIG/galaxy_console.py $PROD
cp $CONFIG/bootstrap.sh $PROD
echo "---------- build done ------------"
