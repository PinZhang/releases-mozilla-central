/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
    File Name:          15.9.5.25-1.js
    ECMA Section:       15.9.5.25 Date.prototype.setUTCMilliseconds(ms)
    Description:
        1.  Let t be this time value.
        2.  Call ToNumber(ms).
        3.  Compute MakeTime(HourFromTime(t), MinFromTime(t), SecFromTime(t), Result(2)).
        4.  Compute MakeDate(Day(t), Result(3)).
        5.  Set the [[Value]] property of the this value to TimeClip(Result(4)).
        6.  Return the value of the [[Value]] property of the this value.
    Author:             christine@netscape.com
    Date:               12 november 1997
*/
    var SECTION = "15.9.5.25-1";
    var VERSION = "ECMA_1";
    startTest();

    writeHeaderToLog( SECTION + " Date.prototype.setUTCMilliseconds(ms)");

    addNewTestCase( 0, 0, "TDATE = new Date(0);(TDATE).setUTCMilliseconds(0);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(0,0)),
                    LocalDateFromTime(SetUTCMilliseconds(0,0)) );
    addNewTestCase( 28800000,999,
                    "TDATE = new Date(28800000);(TDATE).setUTCMilliseconds(999);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(28800000,999)),
                    LocalDateFromTime(SetUTCMilliseconds(28800000,999)) );
    addNewTestCase( 28800000,-28800000,
                    "TDATE = new Date(28800000);(TDATE).setUTCMilliseconds(-28800000);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(28800000,-28800000)),
                    LocalDateFromTime(SetUTCMilliseconds(28800000,-28800000)) );
    addNewTestCase( 946684800000,1234567,
                    "TDATE = new Date(946684800000);(TDATE).setUTCMilliseconds(1234567);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(946684800000,1234567)),
                    LocalDateFromTime(SetUTCMilliseconds(946684800000,1234567)) );
    addNewTestCase( 946684800000, 123456789,
                    "TDATE = new Date(946684800000);(TDATE).setUTCMilliseconds(123456789);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(946684800000,123456789)),
                    LocalDateFromTime(SetUTCMilliseconds(946684800000,123456789)) );

    addNewTestCase( -2208988800000,123456789,
                    "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(123456789);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,123456789)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,123456789)) );

    addNewTestCase( -2208988800000,123456,
                    "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(123456);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,123456)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,123456)) );

    addNewTestCase( -2208988800000,-123456,
                    "TDATE = new Date(-2208988800000);(TDATE).setUTCMilliseconds(-123456);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(-2208988800000,-123456)),
                    LocalDateFromTime(SetUTCMilliseconds(-2208988800000,-123456)) );

    addNewTestCase( 0,-999,
                    "TDATE = new Date(0);(TDATE).setUTCMilliseconds(-999);TDATE",
                    UTCDateFromTime(SetUTCMilliseconds(0,-999)),
                    LocalDateFromTime(SetUTCMilliseconds(0,-999)) );

test();

function addNewTestCase( initialTime, ms, DateString, UTCDate, LocalDate) {
    DateCase = new Date(initialTime);
    DateCase.setUTCMilliseconds(ms);

    new TestCase( SECTION, DateString+".getTime()",             UTCDate.value,       DateCase.getTime() );
    new TestCase( SECTION, DateString+".valueOf()",             UTCDate.value,       DateCase.valueOf() );

    new TestCase( SECTION, DateString+".getUTCFullYear()",      UTCDate.year,    DateCase.getUTCFullYear() );
    new TestCase( SECTION, DateString+".getUTCMonth()",         UTCDate.month,  DateCase.getUTCMonth() );
    new TestCase( SECTION, DateString+".getUTCDate()",          UTCDate.date,   DateCase.getUTCDate() );

    new TestCase( SECTION, DateString+".getUTCHours()",         UTCDate.hours,  DateCase.getUTCHours() );
    new TestCase( SECTION, DateString+".getUTCMinutes()",       UTCDate.minutes,DateCase.getUTCMinutes() );
    new TestCase( SECTION, DateString+".getUTCSeconds()",       UTCDate.seconds,DateCase.getUTCSeconds() );
    new TestCase( SECTION, DateString+".getUTCMilliseconds()",  UTCDate.ms,     DateCase.getUTCMilliseconds() );

    new TestCase( SECTION, DateString+".getFullYear()",         LocalDate.year,       DateCase.getFullYear() );
    new TestCase( SECTION, DateString+".getMonth()",            LocalDate.month,      DateCase.getMonth() );
    new TestCase( SECTION, DateString+".getDate()",             LocalDate.date,       DateCase.getDate() );

    new TestCase( SECTION, DateString+".getHours()",            LocalDate.hours,      DateCase.getHours() );
    new TestCase( SECTION, DateString+".getMinutes()",          LocalDate.minutes,    DateCase.getMinutes() );
    new TestCase( SECTION, DateString+".getSeconds()",          LocalDate.seconds,    DateCase.getSeconds() );
    new TestCase( SECTION, DateString+".getMilliseconds()",     LocalDate.ms,         DateCase.getMilliseconds() );

    DateCase.toString = Object.prototype.toString;

    new TestCase( SECTION,
                                      DateString+".toString=Object.prototype.toString;"+DateString+".toString()",
                                      "[object Date]",
                                      DateCase.toString() );
}
function MyDate() {
    this.year = 0;
    this.month = 0;
    this.date = 0;
    this.hours = 0;
    this.minutes = 0;
    this.seconds = 0;
    this.ms = 0;
}
function LocalDateFromTime(t) {
    t = LocalTime(t);
    return ( MyDateFromTime(t) );
}
function UTCDateFromTime(t) {
 return ( MyDateFromTime(t) );
}
function MyDateFromTime( t ) {
    var d = new MyDate();
    d.year = YearFromTime(t);
    d.month = MonthFromTime(t);
    d.date = DateFromTime(t);
    d.hours = HourFromTime(t);
    d.minutes = MinFromTime(t);
    d.seconds = SecFromTime(t);
    d.ms = msFromTime(t);

    d.time = MakeTime( d.hours, d.minutes, d.seconds, d.ms );
    d.value = TimeClip( MakeDate( MakeDay( d.year, d.month, d.date ), d.time ) );
    d.day = WeekDay( d.value );

    return (d);
}

function SetUTCMilliseconds( T, MS ) {
    T = Number( T );
    TIME = MakeTime(    HourFromTime(T),
                        MinFromTime(T),
                        SecFromTime(T),
                        MS );
    return( MakeDate( Day(T), TIME ));
}
