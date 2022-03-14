#! /usr/bin/env python
# myqttsense_client.py --- MyQTTSense linux client  -*- mode: Python; coding: utf-8 -*- 
__copyright__ = "Copyright (C) 2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>"
__author__    = "Rytis Paškauskas"
"""MyQTTSense linux client
Author: Rytis Paškauskas
Copyright (C) 2021-2022, Rytis Paškauskas <rytis.paskauskas@gmail.com>
URL: https://github.com/rytis-paskauskas/MyQTTSense
Created: 2021-04-19
Last modified: 2022-03-14 18:28:44 (CET) +0100

Description: MyQTTSense client works in tandem with the MyQTTSense app
(Environmental sensing app based on sht3x sensor). 

The reason for providing a client is to make a record of measurements
provided by the application (the app not keep a history of its
measurements). This client is designed to function with 

1. Multiple server applications;
2. Intermittent connectivity; it takes into account that a device or
broker connection might not be constant.

The client logs all readings into one SQL table per device (identified
by device's MAC address) and it tracks connection by the time stamping
current run. The client manipulates database tables to archive the old
data from previous connections into a table without a timestamp. The
old data is averaged by the hour. The variance and number of data
points in each window are also recorded

To run the client, a dedicated MySQL database must be created
separately, and before running the client. The reason to separate the
database and client tasks are the potentially different access levels
necessary for the two tasks (creating a SQL database may require
higher privileges than those of running a database).

We need complete access this database (read, insert, create, delete
tables) and and read-only access to the `information_schema` database.

Creation of a database, on the other hand, may require a higher level
privileges.

The client configuration must be provided in the project's Kconfig
file. The client-specific settings can be found under the "Client
configuration" menu. It is important that MySQL database credentials
are correctly set.

It is possible to create a database using this file, specifically
runnnig @DBManipulator class' @admin method with MySQL administrator's
credentials:

>>> c=confReader('/path/to/Kconfig', '/path/to/cert/file')
>>> m=DBManipulator(c)
>>> m.admin('username','secret')

Running the client.

The included script `install_client.sh` sets up `systemd` service
called `myqttsense_client.service`. To start/stop/enable/status the
service, do systemctl --user start myqttsense_client.service etc.

To check the status of service, do
systemctl --user status myqttsense_client.service

Or, to check the progress, do
journalctl -f --user

"""
import kconfiglib
import logging
import paho.mqtt.client as mqtt
import mysql.connector as mariadb
from datetime import datetime
from os import path
from sys import argv
from systemd.journal import JournalHandler

def gen_tbl_name_components(topic):
    timestamp=datetime.now().strftime("%Y%m%d%H%M%S")
    basename=str('topic_') + topic.replace(':','').replace('/','_');
    return [basename, timestamp]

def gen_tbl_name_new(tc):
    return tc[0] + '_' + tc[1]

def sql_query_db_exists(name):
    return ("SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = '{}'".format(name))

def sql_query_make_tbl_new(name):
    return ("CREATE TABLE IF NOT EXISTS `{}` ("
            "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "`payload` TEXT NOT NULL,"
            "`created_at` DATETIME DEFAULT CURRENT_TIMESTAMP"
            ") ENGINE = MyISAM CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci"
            ).format(name)

def sql_query_make_tbl_avg(name):
    return ("CREATE TABLE IF NOT EXISTS `{}` ("
            "`id` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "`avg_temperature` FLOAT,"
            "`var_temperature` FLOAT,"
            "`avg_humidity` FLOAT,"
            "`var_humidity` FLOAT,"
            "`num_samples` TINYINT UNSIGNED DEFAULT 0,"
            "`dt_window_cntr` DATETIME DEFAULT NULL"
            ") ENGINE = InnoDB CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci"
            ).format(name)

def sql_query_make_tbl_union(name,csv_list):
    return sql_query_make_tbl_new(name).replace('MyISAM',"MERGE UNION=({}) INSERT_METHOD=LAST".format(csv_list))

def on_message(client,userdata,message):
    # Userdata here is a dict: key=topic, val=SQL table name
    m=str(message.payload.decode('utf-8'))
    t=str(message.topic)
    userdata.prepare(t)
    userdata.insert(t,m)

def echo_message(client,userdata,message):
    m=str(message.payload.decode('utf-8'))
    t=str(message.topic)
    print("Topic: {}\t\tmessage: {}".format(t,m))

def log_connection(client,userdata,flags,rc):    
    userdata.logger.info("Connected to MQTT broker")
    userdata.db_open()
    userdata.logger.info("Connected to SQL database")


def log_disconnect_and_stop(client,userdata,flags):
    userdata.logger.info("MQTT broker connection lost. Stopping")
    userdata.db_close()
    client.loop_stop()

def log_subscribe(client,userdata,mid,granted_qos):
    userdata.logger.info("subscribing")

class confReader:
    def __init__(self,confpath,certpath):
        assert path.isfile(confpath), "'{}' not a valid Kconfig file.\nUsage:\n{} confpath certpath".format(confpath, path.basename(repr(__file__)))
        assert path.isfile(certpath), "'{}' not a valid certificate file.\nUsage:\n{} confpath certpath".format(certpath, path.basename(repr(__file__)))
        self.cert=certpath
        kconf=kconfiglib.Kconfig(confpath)
        # Read various parameters
        self.dbh=kconf.syms['DB_URI'].str_value
        self.usr=kconf.syms['DB_USERNAME'].str_value
        self.pwd=kconf.syms['DB_PASSWORD'].str_value
        self.dbn=kconf.syms['DB_DBNAME'].str_value
        self.tpr=kconf.syms['MQTT_CLIENT_TOPIC_HEADER'].str_value
        tmp = kconf.syms['MQTT_BROKER_URI'].str_value.replace("//","").split(":")
        assert len(tmp)>=2, "Kconfig: MQTT_BROKER_URI must contain port number"
        self.port=int(tmp[-1])
        assert self.port>0, "Kconfig: MQTT_BROKER_URI port read error"
        self.host=tmp[-2]

class DBManipulator:
    def __init__(self,c):
        self.conf=c
        self.T=dict()
        self.cnx=None
        self.cursor=None
        self.logger = logging.getLogger(__name__)
        journald_handler = JournalHandler()
        journald_handler.setFormatter(logging.Formatter('[%(levelname)s] %(message)s'))
        self.logger.addHandler(journald_handler)
        self.logger.setLevel(logging.DEBUG)

    def db_exists(self):
        cnx=mariadb.connect(user=self.conf.usr,password=self.conf.pwd,host=self.conf.dbh)
        cursor=cnx.cursor()
        cursor.execute(sql_query_db_exists(self.conf.dbn))
        res=cursor.fetchall()
        cursor.close()
        cnx.close()
        return True if res else False

    def db_open(self):
        self.cnx=mariadb.connect(user=self.conf.usr,password=self.conf.pwd,database=self.conf.dbn,host=self.conf.dbh)
        self.cursor=self.cnx.cursor()

    def db_close(self):
        self.cursor.close()
        self.cnx.close()

    def prepare(self,t):
        # print("verify topic {}".format(t))
        if t not in self.T.keys():
            tc=gen_tbl_name_components(t)
            # Create summary table:
            cnx = mariadb.connect(user=self.conf.usr,password=self.conf.pwd,host=self.conf.dbh,database='information_schema')
            cursor = cnx.cursor()
            cursor.execute("SELECT `table_name` FROM `tables` WHERE `table_schema`='{}' AND `table_name` LIKE '{}_%'".format(self.conf.dbn,tc[0]))
            tbls =[t[0] for t in cursor.fetchall()]
            cursor.close()
            cnx.close()
            self.cursor.execute(sql_query_make_tbl_avg(tc[0]))
            self.cnx.commit()
            # Need to:
            # If there are more than one detailed table we concatenate them and eliminate duplicates
            # If ther is only one table we don't.
            # The variable tbl names:
            #   The details table if only one details table was found (see tbls)
            #   The table `tmp` if more than one detail tables were found. In this case
            #   we will additionally union all detail tables into `tmp` and
            #   eliminate dupes indexed by the time-stamp `created_by`
            # print("tbls={}".format(tbls))
            tbl=''
            if len(tbls)==1:
                tbl=tbls[0]
            else:
                # print("making a `tmp` table from union ({})".format(",".join(tbls)))  
                tbl='tmp'
                self.cursor.execute("DROP TABLE IF EXISTS `tmp`")
                self.cnx.commit()
                # print("here is the query:\n{}".format(sql_query_make_tbl_union('tmp',",".join(tbls))))
                self.cursor.execute(sql_query_make_tbl_union('tmp',",".join(tbls)))
                self.cnx.commit()
                # eliminate dupes
                self.cursor.execute("DELETE `t1` FROM `tmp` `t1` INNER JOIN `tmp` `t2` WHERE `t1`.`id`<`t2`.`id` AND `t1`.`created_at`=`t2`.`created_at`")
                self.cnx.commit()
            # At this point we add the averages/variances into the basename table
            self.cursor.execute(
                ("INSERT INTO `{}` (`avg_temperature`,`var_temperature`,`avg_humidity`,`var_humidity`,`num_samples`,`dt_window_cntr`) "
                 "SELECT AVG(t), VARIANCE(t), AVG(h), VARIANCE(h), COUNT(dt), TIMESTAMP(DATE(dt), MAKETIME(HOUR(dt),30,0)) FROM ("
                 "SELECT JSON_EXTRACT(payload, '$.measurement.temperature') AS t, JSON_EXTRACT(payload, '$.measurement.humidity') AS h, "
                 "created_at AS dt, CONCAT(DAY(created_at), '-', HOUR(created_at)) as id FROM `{}`) tbl "
                 "GROUP BY id").format(tc[0],tbl))
            self.cnx.commit()
            # clean up
            for tbl in tbls:
                self.cursor.execute("DROP TABLE IF EXISTS {}".format(tbl))
                self.cnx.commit()
            # Create a new current table with the timestamp
            self.cursor.execute(sql_query_make_tbl_new(gen_tbl_name_new(tc)))
            self.cnx.commit()
            self.T[t]=tc
            # Log the new topic, table name, time stamp
            self.logger.info("new topic: %s => `%s`",t,gen_tbl_name_new(tc))

    def insert(self,t,m):
        if t not in self.T.keys():
            abort()
        self.cursor.execute("INSERT INTO `{}` (payload) VALUES (%s)".format(gen_tbl_name_new(self.T[t])), (m,))
        self.cnx.commit()

    def admin(self,admin_user,admin_pwd):
        """
        Create a new database for the client

        The database will be created as specified in the Kconfig: with name DB_DBNAME and permissions given to DB_USERNAME

        Note that this needs to be performed only once.
        Call: admin(username, password) 
        where
        :param username
        :param password
        are MySQL administrator's credentials (i.e. someone who has the right to create a database).
        
        Usage example:
        c=confReader('Kconfig')
        m=DBManipulator(c)
        m.admin('root','secret')
        """
        cnx = mariadb.connect(user=admin_user,password=admin_pwd,host=self.conf.dbh)
        cursor = cnx.cursor()
        cursor.execute("CREATE DATABASE IF NOT EXISTS `{}`".format(self.conf.dbn))
        cnx.commit()
        cursor.execute("GRANT SELECT, INSERT, UPDATE, DELETE, CREATE, INDEX, DROP, ALTER, CREATE TEMPORARY TABLES, LOCK TABLES ON `{}`.* TO '{}'@'{}'".format(self.conf.dbn, self.conf.usr,self.conf.dbh))
        cnx.commit()
        cursor.close()
        cnx.close()

def main():
    assert len(argv)>2, "Please provide two command line arguments"
    cfg=confReader(argv[1], argv[2])
    m=DBManipulator(cfg)
    assert m.db_exists(), "MySQL database `{}` does not exist. Please create database first\n".format(cfg.dbn)
    # MQTT client
    listener=mqtt.Client(userdata=m)
    listener.on_message=on_message
    listener.on_connect=log_connection
    listener.on_subscribe=log_subscribe
    listener.on_disconnect=log_disconnect_and_stop
    listener.tls_set(cfg.cert)
    listener.tls_insecure_set(True)
    listener.connect(cfg.host,cfg.port,120)
    listener.subscribe(cfg.tpr + '/#')
    listener.loop_forever()
    listener.disconnect()

if __name__ == "__main__":
    main()
