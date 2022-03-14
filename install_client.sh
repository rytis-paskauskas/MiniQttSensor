#!/bin/bash
# Installs systemd linux client and optionally starts/enables it.
# These settings should work without modification
SYSTEMD_USR=$USER		# alternatively, set your user 
SYSTEMD_GRP=$SYSTEMD_USR	# alternatively set your group
# Default is to run systemd service as user from home:
SYSTEMD_SVC_DIR=${HOME}/.config/systemd/user
SCRIPT_DIR=${HOME}/.local/bin

### Don't modify past this point ###
###

CLIENT=myqttsense_client.py
SERVICE=myqttsense_client.service
BASEDIR=client

install -o ${SYSTEMD_USR} -g ${SYSTEMD_GRP} -m 0754 $BASEDIR/$CLIENT  $SCRIPT_DIR
install -o ${SYSTEMD_USR} -g ${SYSTEMD_GRP} -m 0640 $BASEDIR/$SERVICE $SYSTEMD_SVC_DIR
sed -i "s:_SCRIPT_:${SCRIPT_DIR}/$CLIENT:;s:_ARGV1_:${PWD}/main/Kconfig.projbuild:;s:_ARGV2_:${PWD}/ca_certificates/broker.crt:" ${SYSTEMD_SVC_DIR}/${SERVICE}
systemctl --user daemon-reload

## Optionally start service:
# systemctl --user start $SERVICE

## Optionally enable service:
# systemctl --user enable $SERVICE
