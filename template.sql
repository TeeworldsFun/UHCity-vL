-- MySQL dump 10.17  Distrib 10.3.25-MariaDB, for debian-linux-gnu (x86_64)
--
-- Host: 10.66.173.119    Database: mmohard
-- ------------------------------------------------------
-- Server version	5.7.18-txsql-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `tw_Accounts`
--

DROP TABLE IF EXISTS `tw_Accounts`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_Accounts` (
  `UserID` int(11) NOT NULL AUTO_INCREMENT,
  `Username` varchar(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
  `PasswordHash` varchar(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
  `Rcon` varchar(64) CHARACTER SET utf8 COLLATE utf8_bin DEFAULT NULL,
  `Level` int(11) UNSIGNED NOT NULL DEFAULT 1,
  `ExpPoints` bigint(16) NOT NULL DEFAULT 0,
  `Money` bigint(16) NOT NULL DEFAULT 0,
  `Health` int(11) NOT NULL DEFAULT 10,
  `Armor` int(11) NOT NULL DEFAULT 10,
  `Kills` int(11) NOT NULL DEFAULT 0,
  `HouseID` int(11) NOT NULL DEFAULT 0,
  `VIP` boolean NOT NULL DEFAULT FALSE,
  `Donor` boolean NOT NULL DEFAULT FALSE,
  `Bounty` bigint(16) NOT NULL DEFAULT 0,
  `Arrested` int(11) NOT NULL DEFAULT 0,
  `CreateDate` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`UserID`) USING BTREE,
   KEY Username (`Username`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_Accounts`
--

-- LOCK TABLES `tw_Accounts` WRITE;
/*!40000 ALTER TABLE `tw_Accounts` DISABLE KEYS */;
-- INSERT INTO `tw_Accounts` VALUES (1,'Star','天上的星星',1,'Nope',4,2595550,20,5001,16164,53,131,0,0,'2021-08-04 14:00:00');
/*!40000 ALTER TABLE `tw_Accounts` ENABLE KEYS */;
-- UNLOCK TABLES;

--
-- Table structure for table `tw_Weapons`
--

DROP TABLE IF EXISTS `tw_Weapons`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `tw_Weapons` (
  `UserID` int(11) NOT NULL,
  `Username` varchar(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
  `HammerLvl` int(11) NOT NULL DEFAULT '0',
  `GunLvl` int(11) NOT NULL DEFAULT '0',
  `ShotgunLvl` int(11) NOT NULL DEFAULT '0',
  `GrenadeLvl` int(11) NOT NULL DEFAULT '0',
  `RifleLvl` int(11) NOT NULL DEFAULT '0',
  `MailType` int(11) NOT NULL DEFAULT '0',
	`GrenadeSpread` int(11) NOT NULL DEFAULT '0',
	`GrenadeBounce` int(11) NOT NULL DEFAULT '0',
	`GrenadeMine` int(11) NOT NULL DEFAULT '0',
	`ShotgunSpread` int(11) NOT NULL DEFAULT '0',
	`ShotgunExplode` int(11) NOT NULL DEFAULT '0',
	`ShotgunStars` int(11) NOT NULL DEFAULT '0',
	`RifleSpread` int(11) NOT NULL DEFAULT '0',
	`RifleSwap;` int(11) NOT NULL DEFAULT '0',
	`RiflePlasma` int(11) NOT NULL DEFAULT '0',
	`GunSpread` int(11) NOT NULL DEFAULT '0',
	`GunExplode` int(11) NOT NULL DEFAULT '0',
	`GunFreeze` int(11) NOT NULL DEFAULT '0',
	`HammerWalls` int(11) NOT NULL DEFAULT '0',
	`HammerShot` int(11) NOT NULL DEFAULT '0',
	`HammerKill` int(11) NOT NULL DEFAULT '0',
	`HammerExplode` int(11) NOT NULL DEFAULT '0',
	`NinjaPermanent` int(11) NOT NULL DEFAULT '0',
	`NinjaStart` int(11) NOT NULL DEFAULT '0',
	`NinjaSwitch` int(11) NOT NULL DEFAULT '0',
	`NinjaFly` int(11) NOT NULL DEFAULT '0',
	`NinjaBomber` int(11) NOT NULL DEFAULT '0',
	`EndlessHook` int(11) NOT NULL DEFAULT '0',
	`HealHook` int(11) NOT NULL DEFAULT '0',
	`BoostHook` int(11) NOT NULL DEFAULT '0',
	`PushAura` int(11) NOT NULL DEFAULT '0',
	`PullAura` int(11) NOT NULL DEFAULT '0',
  PRIMARY KEY (`UserID`) USING BTREE,
  KEY Username (`Username`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 ROW_FORMAT=COMPACT;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `tw_Weapons`
--

-- LOCK TABLES `tw_Weapons` WRITE;
/*!40000 ALTER TABLE `tw_Weapons` DISABLE KEYS */;
-- INSERT INTO `tw_Weapons` VALUES (1,1,1,1,10);
/*!40000 ALTER TABLE `tw_Weapons` ENABLE KEYS */;
-- UNLOCK TABLES;

/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

