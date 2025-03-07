function doGet(e) {
  var sheetAmpWatt = SpreadsheetApp.openById("url-spreadsheet").getSheetByName("AmpWatt_History");
  var sheetKWh = SpreadsheetApp.openById("url-spreadsheet").getSheetByName("kWh_Realtime");

  // Tambahkan baris baru untuk Ampere & Watt (historical)
  var timestamp = new Date();
  sheetAmpWatt.appendRow([
    timestamp,
    e.parameter.m1_a, e.parameter.m1_w,
    e.parameter.m2_a, e.parameter.m2_w,
    e.parameter.m3_a, e.parameter.m3_w,
    e.parameter.m4_a, e.parameter.m4_w
  ]);

  // Perbarui kWh di sel tetap
  sheetKWh.getRange("B2").setValue(e.parameter.m1_kwh);
  sheetKWh.getRange("B3").setValue(e.parameter.m2_kwh);
  sheetKWh.getRange("B4").setValue(e.parameter.m3_kwh);
  sheetKWh.getRange("B5").setValue(e.parameter.m4_kwh);

  return ContentService.createTextOutput("Data berhasil diperbarui!");
}
