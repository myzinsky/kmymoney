#!/usr/bin/perl -w

#***************************************************************************
# fy-qif.pl - description
# -------------------
# begin : Sat 10 Feb 2012
# copyright : (C) 2012 by Joe W. Byers
# email : financialseal@financialseal.com
#
#***************************************************************************/
#
#***************************************************************************
#*                                                                         *
#* This program is free software; you can redistribute it and/or modify    *
#* it under the terms of the GNU General Public License as published by    *
#* the Free Software Foundation; either version 2 of the License, or       *
#* (at your option) any later version.                                     *
#*                                                                         *
#***************************************************************************/

# Simple script to download security prices from finance.yahoo and create a 
# qif format for KMyMoney.
# Run by './fy-qif-history.pl' .
# You are required to specify a directory and file location in line  57.
# You should edit the list of security symbols in line 42.
# This file can be scheduled as a cron job if needed

# Output format -
# !Type:Prices
# "HJU8.BE",61.62,"23.12.09"
# ^

# This uses perl-Finance-YahooQuote
# Copyright (C) 2002 Dirk Eddelbuettel <edd@debian.org>, and GPL'ed
# Based on the original example by Dj Padzensky
#
# $Id: yahooquote,v 1.2 2002/12/24 17:50:28 edd Exp $

#Todo: create a file read to read in security symbols

#use strict;
use Getopt::Long;
use Finance::QuoteHist::Yahoo;
use HTTP::Date;
use DateTime;
use DateTime::Format::Flexible;

#@syms=qw("WMB AAON");
# CSCO EP FLVCX FSCGX ISSC MSFT NOEQX NOSIX NTCHX PG RHT SLR
#	    FCNTX FPURX FJPNX FSENX OSMVX ALU TCLFX FSLBX FDFAX FDIVX ARTMX EPD FDGFX OBFVX TCLFX 
#	    OBBC FDIKX FDGFX");
$padlen = 2;


#print "@syms";

my $verbose = 0;
my $q = new Finance::QuoteHist::Yahoo (
	symbols    => [qw("WMB AAON CSCO EP FLVCX FSCGX ISSC MSFT NOEQX NOSIX NTCHX PG RHT SLR
	    FCNTX FPURX FJPNX FSENX OSMVX ALU TCLFX FSLBX FDFAX FDIVX ARTMX EPD FDGFX OBFVX TCLFX 
	    OBBC FDIKX FDGFX")],
        start_date => '1/1/2010',
	end_date   => 'today'
	);

$file = "/var/datadl/quoteshist.csv";
open(FH, ">> $file") || die $!;


#values
foreach $row ($q->quotes()) {
  print FH "!Type:Prices\n";
    ($symbol, $date, $close) = @$row;
#print "$symbol\n";
      print FH "Y$symbol" . ",";
      $yr=substr($date,0,4);
      $mm = substr($date,5,2);
      $mm = sprintf("%0${padlen}d", $mm);
      $dd = substr($date,8,2);
      #$dd=sprintf("%0${padlen}d", $d);
      #print "$date\n";
      print FH "  $mm/$dd/$yr" . "\n";
      print FH "I$close\n";
      print FH "^\n";

#  print @$row["$symbol"]; print "=="; print @$row[ "$date"] ; 

}
close(FH);
