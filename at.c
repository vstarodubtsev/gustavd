#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

#include "main.h"
#include "at.h"

#define QUECTEL_5G

#define FW_VERSION_							"1.5.2"
#define IMEI_								"352812192643726"
#define IMSI_								"262076983126791"
#define ICCID_								"89262070872643044636"

#ifdef QUECTEL_5G

#define MANUFACTURER_						"QUECTEL BY GUSTAV"
#define MODEL_								"RM500U"
#define SUBEDITION_							"V05"

#else

#define MANUFACTURER_						"SIMCOM BY GUSTAV"
#define MODEL_								"A7909E"
#define SUBEDITION_							"B03V01"

#endif

enum cpms_t
{
	CPMS_SM,
	CPMS_ME
} cpms = CPMS_SM;

enum network_mode_t
{
	NET_MODE_AUTO,
	NET_MODE_NR,
	NET_MODE_LTE,
	NET_MODE_UMTS
} net_mode = NET_MODE_AUTO;

int echo = 0;
int enqueueUssd = 0;
int waitPdu = 0;

const char* USSD_RESP = "+CUSD: 2,\"42616c616e733a20302e343920736f276d2e\",-12";

static bool isPdu1a(const char *str)
{
	if (str == NULL || *str == 0) {
		return false;
	}

	while (*str) {
		if (!isxdigit(*str)) {

			if (*str == 0x1a)
			{
				return true;
			}

			return false;
		}
		str++;
	}

	return false;
}

void at_read_line_cb(const char *line)
{
	if (echo)
	{
		tty_write_line(line);
	}

	if (waitPdu)
	{
		waitPdu = 0;
		tty_write_line(isPdu1a(line) ? "OK" : "ERROR");

		return;
	}

	if (!strcasecmp(line, "AT") ||
		!strcasecmp(line, "AT+CMEE=1") ||
		!strncasecmp(line, "AT+CFUN=", 8) ||
		!strcasecmp(line, "AT+CREG=0") ||
		!strcasecmp(line, "AT+CEREG=0") ||
		!strcasecmp(line, "AT+C5GREG=0") ||
		!strcasecmp(line, "AT+DIALMODE=0") ||
		!strcasecmp(line, "AT+CNETCI=0") ||
		!strcasecmp(line, "AT+CNMI=2,1") ||
		!strcasecmp(line, "AT+USBNETIP=1") ||
		!strncasecmp(line, "AT+CNBP=", 8) ||
		!strncasecmp(line, "AT+CGDCONT=1,", 13) ||
		!strncasecmp(line, "AT+ZGDCONT=1,", 13) ||
		!strncasecmp(line, "AT+CGAUTH=", 10) ||
		!strncasecmp(line, "AT+AUTOAPN=", 11) ||
		!strcasecmp(line, "AT+CMGF=0") ||
		!strcasecmp(line, "AT+COPS=2") ||
		!strcasecmp(line, "AT*CELL=0") ||
		!strcasecmp(line, "AT+CGACT=1,1") ||
		!strcasecmp(line, "AT+CGACT=0") ||
		!strcasecmp(line, "AT+CGATT=0") ||
		!strcasecmp(line, "AT+ZGACT=1,1") ||
		!strcasecmp(line, "AT+COPS=3,0") ||
		!strcasecmp(line, "AT+QCFG=\"autoapn\",0") ||
		!strncasecmp(line, "AT+QCFG=\"ims\",", 14) ||
		!strncasecmp(line, "AT+QNWLOCK=", 11) ||
		!strcasecmp(line, "AT+QSIMDET=1,1") ||
		!strcasecmp(line, "AT+QIACT=1") ||
		!strcasecmp(line, "AT+QNETDEVCTL=1,1,1") ||
		!strncasecmp(line, "AT+CGPIAF=", 10) ||
		!strncasecmp(line, "AT+QICSGP=", 10) ||
		!strcasecmp(line, "AT+CMEE=1")) {
		;
	} else if (!strcasecmp(line, "ATE1")) {
		echo = 1;
	} else if (!strcasecmp(line, "ATE0")) {
		echo = 0;
	} else if (!strcasecmp(line, "ATI")) {
		tty_write_line("Manufacturer: " MANUFACTURER_);
		tty_write_line("Model: " MODEL_);
		tty_write_line("Revision: V1.0.009");
		tty_write_line("IMEI: " IMEI_);
	} else if (!strcasecmp(line, "AT+SIMCOMATI")) {
		tty_write_line("Manufacturer: " MANUFACTURER_);
		tty_write_line("Model: " MODEL_);
		tty_write_line("Revision: " FW_VERSION_);
		tty_write_line("IMEI: " IMEI_);
	} else if (!strcasecmp(line, "ATI;+CSUB")) {
		tty_write_line(MANUFACTURER_);
		tty_write_line(MODEL_);
		tty_write_line("Revision: " FW_VERSION_);
		tty_write_line("SubEdition: " SUBEDITION_);
	} else if (!strcasecmp(line, "AT+GSN")) {
		tty_write_line(IMEI_);
	} else if (!strcasecmp(line, "AT+CGMR")) {
		tty_write_line("+CGMR: " FW_VERSION_);
	} else if (!strcasecmp(line, "AT+CMEE?")) {
		tty_write_line("+CMEE: 1");
	} else if (!strcasecmp(line, "AT+CSUB")) {
		tty_write_line("+CSUB: " SUBEDITION_);
	} else if (!strcasecmp(line, "AT+UIMHOTSWAPLEVEL?")) {
		tty_write_line("+UIMHOTSWAPLEVEL: 1");
	} else if (!strcasecmp(line, "AT+UIMHOTSWAPON?")) {
		tty_write_line("+UIMHOTSWAPON: 1");
	} else if (!strcasecmp(line, "AT+USBNETIP?")) {
		tty_write_line("USBNETIP=1");
	} else if (!strcasecmp(line, "AT+CMGF?")) {
		tty_write_line("+CMGF: 0");
	} else if (!strcasecmp(line, "AT+CPIN?")) {
		tty_write_line("+CPIN: READY");
	} else if (!strcasecmp(line, "AT+CNBP?")) {
		tty_write_line("+CNBP: 0X000700000FEB0180,0X000007FF3FDF3FFF");
	} else if (!strcasecmp(line, "AT+CICCID")) {
		tty_write_line("+ICCID: " ICCID_);
	} else if (!strcasecmp(line, "AT+CSIM=10,\"0020000100\"")) {
		tty_write_line("+CSIM:4,\"63C3\"");
	} else if (!strcasecmp(line, "AT+QPINC=\"SC\"")) {
		tty_write_line("+QPINC: \"SC\",3,10");
	} else if (!strcasecmp(line, "AT+CIMI")) {
		tty_write_line(IMSI_);
	} else if (!strcasecmp(line, "AT+CNUM")) {
		tty_write_line("+CME ERROR: 4");
		return;
	} else if (!strcasecmp(line, "AT+CSPN?")) {
		tty_write_line("+CSPN: \"Virtual\",0");
	} else if (!strcasecmp(line, "AT+CREG?")) {
		tty_write_line("+CREG: 0,0");
	} else if (!strcasecmp(line, "AT+CEREG?")) {
		tty_write_line("+CEREG: 0,1");
	} else if (!strcasecmp(line, "AT+C5GREG?")) {
		tty_write_line("+C5GREG: 0,0");
	} else if (!strcasecmp(line, "AT+CGCONTRDP")) {
		tty_write_line("+CGCONTRDP: 1,5,\"test.MNC002.MCC255.GPRS\",\"10.36.130.148\",\"\",\"10.97.52.68\",\"10.97.52.76\",\"\",\"\",0,0");
	} else if (!strcasecmp(line, "AT+CSQ")) {
		tty_write_line("+CSQ: 23,99");
	} else if (!strcasecmp(line, "AT+CGATT?")) {
		tty_write_line("+CGATT: 1");
	} else if (!strcasecmp(line, "AT+CPSI?")) {
		if (net_mode == NET_MODE_UMTS) {
			tty_write_line("+CPSI: WCDMA,Online,252-02,0x2612,-294967296,WCDMA IMT 2000,437,10687,0,-3,-83,-32768,-83,-15");
		} else {
			tty_write_line("+CPSI: LTE,Online,252-02,0x260A,196089506,299,EUTRAN-BAND7,2850,5,5,21,47,43,17");
		}
	} else if (!strcasecmp(line, "AT+COPS?")) {
		if (net_mode == NET_MODE_UMTS) {
			tty_write_line("+COPS: 0,0,\"GustaFon\",6");
		} else {
			tty_write_line("+COPS: 0,0,\"GustaFon\",9");
		}
	} else if (!strcasecmp(line, "AT+ZCAINFO?")) {
		if (net_mode != NET_MODE_UMTS) {
			tty_write_line("+ZCAINFO: 299,7,17758,2850,10;341,1,3,1802,20");
		}
	} else if (!strcasecmp(line, "AT+COPS=0")) {
		tty_write_line("+XACTIVATE: 1");
		usleep(500);
		tty_write_line("+XACTIVATE: 2");
		sleep(2);
	} else if (!strcasecmp(line, "AT+COPS=?")) {
		sleep(1);
		tty_write_line("+COPS: "
			"(1, \"GustaFon GUS\", \"GustaFon\", \"25202\", 2),"
			"(1, \"Tele2 EU\", \"Tele2\", \"25220\", 2),"
			"(1, \"GustaFon GUS\", \"GustaFon\", \"25202\", 7),"
			"(1, \"Beeline\", \"Beeline\", \"25299\", 7),"
			"(1, \"Tele2 EU\", \"Tele2)(\", \"25220\", 7),"
			"(1, \"YOTA:)\", \"YOTA\", \"25211\", 7),"
			"(1, \"MTS GUS\", \"MTS GUS\", \"25201\", 7),"
			",(0,1,2,3,4),(0,1,2)");
	} else if (!strcasecmp(line, "AT+CGPADDR=1")) {
		if (enqueueUssd) {
			enqueueUssd = 0;
			tty_write_line(USSD_RESP);
		}

		tty_write_line("+CGPADDR: 1, \"10.36.130.148\"");
	} else if (!strcasecmp(line, "AT+CPMUTEMP")) {
		tty_write_line("+CPMUTEMP: 36");
	} else if (!strcasecmp(line, "AT+CNETCI?")) {
		if (net_mode != NET_MODE_UMTS) {
			tty_write_line("+CNETCISRVINFO: MCC-MNC: 252-02,TAC: 9738,cellid: 196089506,rsrp: 47,rsrq: 21, pci: 299,earfcn: 2850");
			tty_write_line("+CNETCINONINFO: 0,MCC-MNC: 000-00,TAC: 0,cellid: -1,rsrp: 23,rsrq: 0,pci: 195,earfcn: 1602");
			tty_write_line("+CNETCINONINFO: 1,MCC-MNC: 000-00,TAC: 0,cellid: -1,rsrp: 31,rsrq: 17,pci: 92,earfcn: 1602");
		}
		tty_write_line("+CNETCI: 0");
	} else if (!strcasecmp(line, "AT+SIGNS")) {
		if (net_mode != NET_MODE_UMTS) {
			tty_write_line("+RSRP0: -109");
			tty_write_line("+RSRP1: -112");
			tty_write_line("+RSRQ0: -11");
			tty_write_line("+RSRQ1: -11");
			tty_write_line("+RSSI0: -61");
			tty_write_line("+RSSI1: -64");
		}
	} else if (!strcasecmp(line, "AT+DIALMODE?")) {
		tty_write_line("+DIALMODE: 0");
	} else if (!strcasecmp(line, "AT+QCFG=\"nat\"")) {
		tty_write_line("+QCFG: \"nat\",1");
	} else if (!strcasecmp(line, "AT+QCFG=\"ethernet\"")) {
		tty_write_line("+QCFG: \"ethernet\",0");
	} else if (!strcasecmp(line, "AT+QCFG=\"pcie/mode\"")) {
		tty_write_line("+QCFG: \"pcie/mode\",0");
	} else if (!strcasecmp(line, "AT+QCFG=\"usbnet\"")) {
		tty_write_line("+QCFG: \"usbnet\",0");
	} else if (!strcasecmp(line, "AT+QUIMSLOT?")) {
		tty_write_line("+QUIMSLOT: 1");
	} else if (!strcasecmp(line, "AT+QCCID")) {
		tty_write_line("+QCCID: " ICCID_);
	} else if (!strcasecmp(line, "AT+QSPN")) {
		tty_write_line("+QSPN: \"Virtual\",\"Virtual\",\"Virtual\",0,\"25201\"");
	} else if (!strcasecmp(line, "AT+QNETDEVCTL?")) {
		tty_write_line("+QNETDEVCTL: 1,2,1");
		tty_write_line("+QNETDEVCTL: 2,2,0");
	} else if (!strcasecmp(line, "AT+QNWINFO")) {
		if (net_mode == NET_MODE_AUTO) {
			tty_write_line("+QNWINFO: \"FDD LTE\",26203,\"LTE BAND 1\",300");
			tty_write_line("+QNWINFO: \"NR5G-NSA\",26203,\"NR N41\",529950");
		} else if (net_mode == NET_MODE_NR) {
			tty_write_line("+QNWINFO: \"NR5G-SA\",26203,\"NR N41\",529950");
		} else if (net_mode == NET_MODE_LTE) {
			tty_write_line("+QNWINFO: \"FDD LTE\",26202,\"LTE BAND 7\",2850");
		} else if (net_mode == NET_MODE_UMTS) {
			tty_write_line("+QNWINFO: \"HSPA+\",25002,\"WCDMA 2100\",10687");
		}
	} else if (!strcasecmp(line, "AT+QENG=\"servingcell\"")) {
		if (net_mode == NET_MODE_AUTO) {
			tty_write_line("+QENG: \"servingcell\",\"CONNECT\"");
			tty_write_line("+QENG: \"LTE\",\"FDD\",262,03,1212126,118,300,1,5,5,B8FD,-108,-10,-78,4,10,23,19");
			tty_write_line("+QENG: \"NR5G-NSA\",262,03,170,-93,3,-8,529950,41,0,157E,1");
		} else if (net_mode == NET_MODE_NR) {
			tty_write_line("+QENG: \"servingcell\",\"CONNECT\",\"NR5G-SA\",\"TDD\",262,00,C22221001,808,1421AF,504990,41,100,-71,0,27,7,42,1");
		} else if (net_mode == NET_MODE_LTE) {
			tty_write_line("+QENG: \"servingcell\",\"CONNECT\",\"LTE\",\"FDD\",262,02,1951D49,12,2850,7,5,5,260A,-92,-8,-68,20,13,0,31");
		} else if (net_mode == NET_MODE_UMTS) {
			tty_write_line("+QENG: \"servingcell\",\"CONNECT\",\"WCDMA\",262,02,2612,656BAF,10687,166,-84,-8,1,6,0");
		}
	} else if (!strcasecmp(line, "AT+QENG=\"neighbourcell\"")) {
		if (net_mode == NET_MODE_AUTO) {
			tty_write_line("+QENG: \"neighbourcell intra\",\"LTE\",6300,319,-102,-10,26,1,7,-,-,-,-");
			tty_write_line("+QENG: \"neighbourcell inter\",\"LTE\",100,183,-131,-24,0,-13,255,-1,-1,16");
		} else if (net_mode == NET_MODE_NR) {
			tty_write_line("+QENG: \"neighbourcell\",\"NR\",529950,170,-88,-6,7,32");
		} else if (net_mode == NET_MODE_LTE) {
			tty_write_line("+QENG: \"neighbourcell intra\",\"LTE\",300,118,-11,-11,17,1,1,-,-,-,-");
			tty_write_line("+QENG: \"neighbourcell inter\",\"LTE\",6200,297,-106,-16,0,2,255,-1,-1,16");
			tty_write_line("+QENG: \"neighbourcell inter\",\"LTE\",1600,183,-110,-15,0,-2,255,-1,-1,16");
		}
	} else if (!strcasecmp(line, "AT+QTEMP")) {
		tty_write_line("+QTEMP: \"soc-thermal\",\"36\"");
		tty_write_line("+QTEMP: \"pa-thermal\",\"36\"");
		tty_write_line("+QTEMP: \"pa5g-thermal\",\"36\"");
	} else if (!strcasecmp(line, "AT+QCAINFO")) {
		if (net_mode == NET_MODE_AUTO) {
			tty_write_line("+QCAINFO: \"PCC\",6300,50,\"LTE BAND 20\",1,319,-103,-9,-76,8");
			tty_write_line("+QCAINFO: \"SCC\",100,100,\"LTE BAND 1\",1,372,-111,-13,-,6");
			tty_write_line("+QCAINFO: \"SCC\",372750,20,\"NR N3\",2,431,-108,-7,-89,7");
		} else if (net_mode == NET_MODE_NR) {
			tty_write_line("+QCAINFO: \"PCC\",504990,100,\"NR N41\",1,808,-71,0,-57,26");
		} else if (net_mode == NET_MODE_LTE) {
			tty_write_line("+QCAINFO: \"PCC\",300,100,\"LTE BAND 1\",1,118,-108,-10,-79,3");
			tty_write_line("+QCAINFO: \"SCC\",6300,50,\"LTE BAND 20\",1,319,-103,-9,-76,8");
		} else if (net_mode == NET_MODE_UMTS) {
			;
		}
	} else if (!strcasecmp(line, "AT+QANTRSSI?")) {
		if (net_mode == NET_MODE_AUTO || net_mode == NET_MODE_NR) {
			tty_write_line("+QANTRSSI: 1,-,-59,-,-58,-56,-53");
		} else if (net_mode == NET_MODE_LTE) {
			tty_write_line("+QANTRSSI: 2,-74,-79");
		} else if (net_mode == NET_MODE_UMTS) {
			;
		}
	} else if (!strcasecmp(line, "AT+QNWPREFCFG=?")) {
		tty_write_line("+QNWPREFCFG: \"mode_pref\",AUTO:WCDMA:LTE:NR5G:NR5G-SA:NR5G-NSA");
		tty_write_line("+QNWPREFCFG: \"gw_band\",1:2:5:8");
		tty_write_line("+QNWPREFCFG: \"lte_band\",1:2:3:4:5:7:8:20:28:38:40:41:66");
		tty_write_line("+QNWPREFCFG: \"nr5g_band\",1:3:5:7:8:20:28:38:40:41:66:77:78");
		tty_write_line("+QNWPREFCFG: \"all_band_reset\"");
		tty_write_line("+QNWPREFCFG: \"srv_domain\",(0-2)");
		tty_write_line("+QNWPREFCFG: \"voice_domain\",(0-3)");
		tty_write_line("+QNWPREFCFG: \"ue_usage_setting\",(0,1)");
		tty_write_line("+QNWPREFCFG: \"roam_pref\",(0-3)");
		tty_write_line("+QNWPREFCFG: \"cell_blacklist\",(1-3),(0-15),<freq-pci list>");
		tty_write_line("+QNWPREFCFG: \"mode_blacklist\",(0-5)");
		tty_write_line("+QNWPREFCFG: \"rat_acq_order\",NR5G:LTE:WCDMA");
		tty_write_line("+QNWPREFCFG: \"nr5g_band_blacklist\",(0,1),<nr5g_band_blacklist>");
	} else if (!strncasecmp(line, "AT+QNWPREFCFG=\"mode_pref\",", 26)) {
		if (!strcmp(line + 26, "WCDMA")) {
			net_mode = NET_MODE_UMTS;
		} else if (!strcmp(line + 26, "LTE")) {
			net_mode = NET_MODE_LTE;
		} else if (!strcmp(line + 26, "NR5G")) {
			net_mode = NET_MODE_NR;
		}
	} else if (!strncasecmp(line, "AT+QNWPREFCFG=", 14)) {
		;
	} else if (!strncasecmp(line, "AT+CNMP=", 8)) {
		if (!strcmp(line + 8, "14")) {
			net_mode = NET_MODE_UMTS;
		}
	} else if (!strcasecmp(line, "AT+CNMI?")) {
		tty_write_line("+CNMI: 2,1,1,1,1");
	} else if (!strncasecmp(line, "AT+CPMS=\"SM\"",12)) {
		cpms = CPMS_SM;
		tty_write_line("+CPMS: 1,5,1,5,1,5");
	} else if (!strncasecmp(line, "AT+CPMS=\"ME\"",12)) {
		cpms = CPMS_ME;
		tty_write_line("+CPMS: 37,200,37,200,37,200");
	} else if (!strcasecmp(line, "AT+CPMS?")) {
		if (cpms == CPMS_SM) {
			tty_write_line("+CPMS: \"SM\",1,5,\"ME\",37,200,\"ME\",37,200");
		} else {
			tty_write_line("+CPMS: \"ME\",37,200,\"ME\",37,200,\"ME\",37,200");
		}
	} else if (!strcasecmp(line, "AT+CMGL=4")) {
		if (cpms == CPMS_ME) {
			tty_write_line("+CMGL: 0,1,,160");
			tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223081916324218C05000303030100310039002E00300033002E003200300032003200200432002000310039003A00330036002004370430043F043B0430043D04380440043E04320430043D043E00200441043F043804410430043D043804350020043F043B04300442044B0020043F043E00200442043004400438044404430020201300200037003000300020044004430431");
			tty_write_line("+CMGL: 1,1,,160");
			tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223081916324218C050003030302002E000A041D04300441043B04300436043404300439044204350441044C0020043E043104490435043D04380435043C0020002D002004340435043D043504330020043D043000200441044704350442043500200434043E0441044204300442043E0447043D043E002E041204300448002004310430043B0430043D0441003A002000390039");
			tty_write_line("+CMGL: 2,1,,108");
			tty_write_line("07919762020041F7440DD0CDF2396C7CBB01000822308191632421580500030303030031002E003200370020044004430431002E000A000A041F043E043F043E043B043D04380442044C00200441044704350442003A0020007000610079002E006D0065006700610066006F006E002E00720075");
			tty_write_line("+CMGL: 3,1,,160");
			tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C0500031104010421043F043804410430043D043E00200037003000300020044004430431002E0020043F043E00200442043004400438044404430020002204170430043A04300447043004390441044F00210020041B04350433043A043E00220020043704300020043F043504400438043E043400200441002000310039002E00300033002E003200300032");
			tty_write_line("+CMGL: 4,1,,160");
			tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C05000311040200320020043F043E002000310038002E00300034002E0032003000320032002E000A000A0418043D044204350440043D043504420020043D043000200441043A043E0440043E04410442043800200434043E0020003200350020041C043104380442002F044100200431044304340435044200200434043E044104420443043F0435043D0020");
			tty_write_line("+CMGL: 5,1,,160");
			tty_write_line("07919762020041F7400DD0CDF2396C7CBB010008223091916324218C0500031104030434043E00200441043B043504340443044E044904350433043E00200441043F043804410430043D0438044F0020043F043E0020044204300440043804440443002000310038002E00300034002E0032003000320032002E0020000A000A041F043E043F043E043B043D04380442044C002004310430043B0430043D0441003A002000700061");
			tty_write_line("+CMGL: 6,1,,50");
			tty_write_line("07919762020041F7440DD0CDF2396C7CBB010008223091916324211E0500031104040079002E006D0065006700610066006F006E002E00720075");
			tty_write_line("+CMGL: 7,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C407010421002004430441043B04430433043E0439002000AB0414043E043F043E043B043D043804420435043B044C043D044B04390020043D043E043C0435044000BB00200412044B0020043C043E04360435044204350020043F043E0434043A043B044E044704380442044C0020043D043000200412043004480443002000530049004D002D043A");
			tty_write_line("+CMGL: 8,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C407020430044004420443002004350449043500200434043E00200033002D04450020043D043E043C04350440043E0432002E0020041804450020043C043E0436043D043E002004380441043F043E043B044C0437043E043204300442044C002C0020043A043E04330434043000200412044B0020043D043500200445043E04420438044204350020");
			tty_write_line("+CMGL: 9,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40703043E0441044204300432043B044F0442044C002004410432043E04390020043E0441043D043E0432043D043E04390020043D043E043C04350440002C0020043D0430043F04400438043C04350440002C00200434043B044F002004410432044F0437043800200441043E00200441043B0443043604310430043C043800200434043E04410442");
			tty_write_line("+CMGL: 10,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C4070404300432043A0438002C00200438043D044204350440043D043504420020043C04300433043004370438043D0430043C0438002004380020043C0430043B043E0437043D0430043A043E043C044B043C04380020043B044E0434044C043C0438002E000A04170432043E043D043804420435002004380020043E0442043F044004300432043B");
			tty_write_line("+CMGL: 11,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40705044F04390442043500200053004D00530020043F043E002004430441043B043E04320438044F043C0020043E0441043D043E0432043D043E0433043E0020044204300440043804440430002E002004210442043E0438043C043E04410442044C0020043F043E0434043A043B044E04470435043D0438044F00200434043E043F043E043B043D");
			tty_write_line("+CMGL: 12,1,,160");
			tty_write_line("07919762020041F7600DD0CDF2396C7CBB010008224031718101218C050003C40706043804420435043B044C043D043E0433043E0020043D043E043C043504400430002020140020003300300020044004430431043B04350439002E00200415043604350434043D04350432043D0430044F0020043F043B04300442043000202014002000320020044004430431043B044F00200432002004340435043D044C002E0020041F043E");
			tty_write_line("+CMGL: 13,1,,156");
			tty_write_line("07919762020041F7640DD0CDF2396C7CBB0100082240317181012188050003C407070434043A043B044E044704380442044C002000680074007400700073003A002F002F006C006B002E006D0065006700610066006F006E002E00720075002F0069006E006100700070002F006100640064006900740069006F006E0061006C004E0075006D006200650072007300200438043B04380020002A0034003800310023000A");
			tty_write_line("+CMGL: 14,1,,27");
			tty_write_line("07919762020041F7040B919781314259F800084290526173402108041E043A04300439");
			tty_write_line("+CMGL: 24,1,,159");
			tty_write_line("07919762020041F7440B919780314257F8000842211131754421880500033B0701041F04400435043404320438043604430020043204410435003A00200432043004410020043E0441043A043E0440043104380442000A041F043504470430043B044C043D043E04390020044204300439043D044B0020043E0431044A044F0441043D0435043D044C0435002E000A041A0430043A043E043500200433043E0440044C");
			tty_write_line("+CMGL: 25,1,,159");
			tty_write_line("07919762020041F7440B919780314257F80008422111317545218C0500033B0702043A043E04350020043F04400435043704400435043D044C0435000A04120430044800200433043E04400434044B04390020043204370433043B044F0434002004380437043E0431044004300437043804420021000A042704350433043E00200445043E04470443003F002004410020043A0430043A043E044E002004460435043B044C044E");
			tty_write_line("+CMGL: 26,1,,159");
			tty_write_line("07919762020041F7440B919780314257F80008422111317555218C0500033B0703000A041E0442043A0440043E044E00200434044304480443002004320430043C002004410432043E044E003F000A041A0430043A043E043C044300200437043B043E0431043D043E043C044300200432043504410435043B044C044E002C000A0411044B0442044C0020043C043E043604350442002C0020043F043E0432043E04340020043F");
			tty_write_line("+CMGL: 27,1,,159");
			tty_write_line("07919762020041F7440B919780314257F80008422111317575218C0500033B0704043E04340430044E0021000A0421043B0443044704300439043D043E00200432043004410020043A043E043304340430002D0442043E0020043204410442044004350442044F002C000A04120020043204300441002004380441043A044004430020043D04350436043D043E044104420438002004370430043C04350442044F002C000A042F");
			tty_write_line("+CMGL: 28,1,,159");
			tty_write_line("07919762020041F7440B919780314257F80008422111317585218C0500033B07050020043504390020043F043E04320435044004380442044C0020043D04350020043F043E0441043C0435043B003A000A041F044004380432044B0447043A04350020043C0438043B043E04390020043D0435002004340430043B00200445043E04340443003B000A04210432043E044E0020043F043E04410442044B043B0443044E00200441");
			tty_write_line("+CMGL: 29,1,,159");
			tty_write_line("07919762020041F7440B919780314257F80008422111317595218C0500033B07060432043E0431043E04340443000A042F0020043F043E044204350440044F0442044C0020043D04350020043704300445043E04420435043B002E000A0415044904350020043E0434043D043E0020043D043004410020044004300437043B044304470438043B043E002E002E002E000A041D043504410447043004410442043D043E04390020");
			tty_write_line("+CMGL: 30,1,,57");
			tty_write_line("07919762020041F7440B919780314257F8000842211131850021260500033B070704360435044004420432043E04390020041B0435043D0441043A043804390020");
		} else
		{
			tty_write_line("+CMGL: 0,1,,67");
			tty_write_line("07919731899699F3040b919780514257f800085210223250138230042d0442043e0442002004300431043e043d0435043d0442002004370432043e043d0438043b002004320430043c002e");
		}
	} else if (!strcasecmp(line, "AT+CMGF?")) {
		tty_write_line("+CMGF: 0");
	} else if (!strncasecmp(line, "AT+CUSD=1,", 10)) {
		enqueueUssd = 1;
		sleep(1);
	} else if (!strncasecmp(line, "AT+CMGS=", 8)) {
		waitPdu = 1;
		return;
	} else
	{
		tty_write_line("ERROR");
		return;
	}

	tty_write_line("OK");
}
