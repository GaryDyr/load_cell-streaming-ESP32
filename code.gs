C++
/*
   Works with Scale_ESP32 Google Sheet
   Generates 3 columns/run with up to 500 time, wt pairs.
   Row 1 contains headers; row 2, is date data was transferred in first column with 
   the scale factor (unlabeled) in next colunm.
   Row 3 begins the data in columns: times, wts, and "instantaneous flow rate.
   Headers are automatically added.
   By more brute force reckoning, rather true understanding some of the stuff
   that translates objects into real values was developed. Code reads multiple arrays.
   See fakeGet for the set up. fakeGet() is the way to test the code in the script console
   without sending actual data.

   From perspective of ESP32 code, doGet seems counter intuitive to doPost.
   It processes the context stream, in this case only the "status" key, and sends
   response back to the ESP32, hence the doGet().
*/
var timeZone = "CST"; //see https://www.timeanddate.com/time/zones/
var dateTimeFormat = "MM/dd/yyyy HH:mm:ss";
var column;
var resultType = "text"; //text"; for testing in web page, set to "html" or set to "text" for ESP return

//Google sheet ID 
GS_ID = "GetandPlaceYourGoogleSheetIDhere";
/*
  HOW TO GET FROM SPREADSHEET NAME IN GOOGLE DRIVE:
    var FileIterator = DriveApp.getFilesByName(FileNameString);
    while (FileIterator.hasNext()) {
      var file = FileIterator.next();
      if (file.getName() == FileNameString) {
        var ss = SpreadsheetApp.open(file);
        var fileID = file.getId();
      }    
    }
*/

//open the spreadsheet using the Id obtained from sheet url
var ss = SpreadsheetApp.openById(GS_ID);
Logger.log('sheet name is: ' + ss.getName());
//get sheet to add data to
var sheet = ss.getSheetByName("Sheet1"); 

function fakeGet() {
  //used to Debug Google Script and Sheets; in the Web console; choose this function under Debug.
  //uses the form of underlying doGet process. 
  //parameter - An object of key/value pairs that correspond to the request parameters. Only the first value 
  //is returned for parameters that have multiple values.
  //contextPath: Not used here. Always empty string.
  //contentLength: Always returns -1 for doGet.
  //queryString: The value of the query string portion of the URL. not used
  //parameters: An object similar to e.parameter, but with an array of values for each key
  var eventObject = 
    {
      "parameter": {"scalef":"214.1"},
      "contextPath": "",
      "contentLength": -1,
      "queryString":"exec?scalefactor=[214.1]&times=[0.00,10.00,20.00,30.00,40.00]&wts=[1.00,12.02,13.00,6.00,20.00]",
      "parameters":{"scalefactor":["214.1"],
      "times":'["0.00","10.00","20.00","30.00","40.00"]',
       "wts":'["1.00","12.00","13.02","16.00","20.00"]'}
    }
    doGet(eventObject);
}

function doGet(e) { 
  //next lines used for testing only if badly formed GET url
  //sheet.getRange(10,6).setValue(JSON.stringify(e)); 
  //sheet.getRange(11,6).setValue(JSON.stringify(e.queryString));
  //e is the url as JSON object
  var result = 'doGet Ok '; // default result
  Logger.log("Starting");
  Logger.log(JSON.stringify(e));
  var lastColumn = sheet.getLastColumn();
  var newColumn = lastColumn + 1
  if (e.parameters == 'undefined') {
    result = 'No Parameters';
    sheet.getRange(4,newColumn).setValue(result);
  }
  //Logger.log('parameters: ' + e.parameters); //only indicates object
  // if ((e.parameter !== '') && (e.parameters !== "undefined")) {
  if ((e.parameters !== "undefined")) {  
    var tempt = JSON.parse(e.parameters.times);
    var tempw = JSON.parse(e.parameters.wts);
    //for testing using parse...
    //var pq = e.queryString;
    //Logger.log("parsed query: " + JSON.stringify(parseQuery(pq)));
    //Logger.log("value: " + pq.times)
    count = tempt.length; 
    for (var i=0; i<count; i++) {
      sheet.getRange(i+3, newColumn).setValue(tempt[i]); 
      sheet.getRange(i+3, newColumn+1).setValue(tempw[i]);   
    } 
    
    if ("scalefactor" in e.parameters) {
      scale_factor = e.parameters["scalefactor"][0];
      result += "found scale factor,";
    } else {
      scale_factor = 0.0;
    }
    //put headers
    Logger.log('scale factor value is: ' + scale_factor);
    var rowData = [];      
    var lastCell = sheet.getRange(1, newColumn);
    var HeaderRow=["Time, s", "Weight, g", "Flow Rate, g/s."];
    for (var i = 0; i<HeaderRow.length; i++) {
      rowData[i] = HeaderRow[i];
    }
    var rng = sheet.getRange(2,newColumn,1,HeaderRow.length)
    rng.setValues([rowData]); 
    var rowFont = ["bold", "bold", "bold"];
    rng.setFontWeights([rowFont]);
    rng.setWrapStrategy(SpreadsheetApp.WrapStrategy.WRAP);
  
    var dtime;
    var timediff;
    var wtdiff;   
    if (e.parameter.scalefactor == 'undefined') {
      sheet.getRange(newRow, 2).setValue("undefined");
      result += " Missing scale_factor,";
    } else {
      dtime = Utilities.formatDate(new Date(), timeZone, dateTimeFormat);
      sheet.getRange(1,newColumn).setValue(dtime);
      sheet.getRange(1,newColumn+1).setValue(scale_factor);
    }
    //calculate wt change/s
    var rate;
    var offset=4;
    for (var i=0; i<count-1; i++) {
      timediff = tempt[i+1] - tempt[i];
      wtdiff = tempw[i+1] - tempw[i];
      rate = 1.0*(wtdiff/timediff); //in seconds
      sheet.getRange(i+offset, newColumn+2).setValue(rate.toFixed(3));
    }
                 
    // Because we invoked the script as a web app, it requires we return a ContentService object.
  } else {
      result = "missing data";
  }
  return returnContent(result);
}

// Remove leading and trailing single or double quotes
//did not work here for arrays, may not have used correctly, but did not need.
//function stripQuotes(value) {
 //return value.replace(/^["']|['"]$/g, "");
 //}
 
 //not used
function parseQuery(queryString) {
   var query = {};
   //fyi: querysting is everything after exec?
   var pairs =  (queryString[0] === '?' ? queryString.substr(1) : queryString).split('&');
   //Logger.log('query pairs are: ' + pairs);
    for (var i = 0; i < pairs.length; i++) {
       var pair = pairs[i].split('=');
       query[decodeURIComponent(pair[0])] = decodeURIComponent(pair[1] || '');
    }
   return query;
} 

function returnContent(result) {
  //Make sure to change the output for result_type at top of file, depending on destination or testing.
  if (resultType == "html") {
    //To test, copy the Google sheet script url when authenticating and
    //paste url in browser. After "exec" if deployed as New Deployment Web App, or 
    // after "dev", if using "Test Deployment", add ? + the datastring(no quotes)  
    var MyHtml = "<!doctype html><html><body><h1>Content returned from script</h1><p>" + result + "</p></body></html>";
    // another example 
    //var MyHtlm = "<!doctype html><html><body><h1>Content returned from script</h1><p>" + result + "</p><p>" + e.parameter.status + "</p></body></html>");
    HtmlService.createHtmlOutput(MyHtml); 
   }    
  if (resultType == "text") {
    //used for sending data back to ESP32
    Logger.log('result is ' + result);
    ContentService.createTextOutput(result);
   }
 }   

// best answer yet for next blank row in column: https://stackoverflow.com/questions/26056370/find-lastrow-of-column-c-when-col-a-and-b-have-a-different-row-size

function lastRowIndexInColumn(column) {
   //column must be column letter, e.g. "A"
   //returns last used cell, not next cell
   //fails if there are spaces. If spaces used .getMaxRows()
   var lastRow = sheet.getLastRow();
   if (lastRow > 1) {
     var values = sheet.getRange(column + "1:" + column + lastRow).getValues();
     // format for loop allowed, with double test condition; allowed  and" (;" if var previously defined
     for (; values[lastRow-1] == "" && lastRow > 0; lastRow--) { }
   }
   //to return last value rather than index: return values[lastRow-1)
   return lastRow;
 }

function getActiveSheetId(){
  //Not used directly...utility for editing code.
}

  var id  = SpreadsheetApp.getActiveSheet().getSheetId();
  Logger.log(id.toString());