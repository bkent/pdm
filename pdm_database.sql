create database pidoorman;

drop table pdm_logs;

create table pdm_logs(
id INT NOT NULL AUTO_INCREMENT, 
PRIMARY KEY(id),
faccode VARCHAR(16),
cardno VARCHAR(64),
logdt datetime,
accresult char,
location int(2)
);

alter table pdm_logs add column location int(2);


drop table pdm_cards;

create table pdm_cards(
id INT NOT NULL AUTO_INCREMENT, 
PRIMARY KEY(id),
firstname VARCHAR(16),
surname VARCHAR(16),
faccode VARCHAR(16),
cardno VARCHAR(64),
validfrom datetime,
validuntil datetime,
scheduleid int(11),
imagepath VARCHAR(64)
);

alter table pdm_cards add column scheduleid int(11);
alter table pdm_cards add column imagepath VARCHAR(64);

CREATE TABLE IF NOT EXISTS pdm_users (
  id INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (id),
  user_name varchar(64) NOT NULL,
  short_name varchar(16) NOT NULL,
  password varchar(64) NOT NULL,
  flag varchar(64) DEFAULT NULL
);

CREATE TABLE IF NOT EXISTS pdm_schedule (
  id INT NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (id),
  schedname varchar(64) NOT NULL,
  schedstart datetime,
  schedend datetime
);

create table pdm_locations(
id INT NOT NULL AUTO_INCREMENT, 
PRIMARY KEY(id),
location VARCHAR(64)
);