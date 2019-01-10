<?php

// CONFIG

$logfile = "log.txt";
$latest = array("SWITCH" => 1.1, "TH16" => 1.1);

// Functions

function check_header($name, $value = false) {
    if(!isset($_SERVER[$name])) {
        return false;
    }
    if($value && $_SERVER[$name] != $value) {
        return false;
    }
    return true;
}

function sendFile($path) {
    header($_SERVER["SERVER_PROTOCOL"].' 200 OK', true, 200);
    header('Content-Type: application/octet-stream', true);
    header('Content-Disposition: attachment; filename='.basename($path));
    header('Content-Length: '.filesize($path), true);
    header('x-MD5: '.md5_file($path), true);
    readfile($path);
}

// MAIN

// prepare log
$log = date("d.m.Y H:m:s") . " " . $_SERVER['REMOTE_ADDR'] . " ";

// debug log
//file_put_contents($logfile, print_r($_SERVER, TRUE), FILE_APPEND);

// set output to plain text, not html
header('Content-type: text/plain; charset=utf8', true);

// check if the user agent matches esp update library
if(!check_header('HTTP_USER_AGENT', 'ESP8266-http-Update')) {
    header($_SERVER["SERVER_PROTOCOL"].' 403 Forbidden', true, 403);
    echo "This service is only available for ESP8266 updater!\n\n";
    echo "For more information visit: https://github.com/Rup0rt/Sonoff-Smart-Switch-Amazon-Alexa\n";
    $log .= "Wrong user agent: " . $_SERVER['HTTP_USER_AGENT'] . "\n";
    file_put_contents($logfile, $log, FILE_APPEND);
    exit();
}

// check if all header data is available
if(
    !check_header('HTTP_X_ESP8266_STA_MAC') ||
    !check_header('HTTP_X_ESP8266_AP_MAC') ||
    !check_header('HTTP_X_ESP8266_FREE_SPACE') ||
    !check_header('HTTP_X_ESP8266_SKETCH_SIZE') ||
    !check_header('HTTP_X_ESP8266_CHIP_SIZE') ||
    !check_header('HTTP_X_ESP8266_SDK_VERSION') ||
    !check_header('HTTP_X_ESP8266_VERSION')
) {
    header($_SERVER["SERVER_PROTOCOL"].' 403 Forbidden', true, 403);
    echo "Missing headers...\n\n";
    echo "This service is only available for ESP8266 updater!\n\n";
    echo "For more information visit: https://github.com/Rup0rt/Sonoff-Smart-Switch-Amazon-Alexa\n";
    $log .= "Missing headers!\n";
    file_put_contents($logfile, $log, FILE_APPEND);
    exit();
}

// get data
$mac = $_SERVER['HTTP_X_ESP8266_STA_MAC'];
$ap = $_SERVER['HTTP_X_ESP8266_AP_MAC'];
$freespace = $_SERVER['HTTP_X_ESP8266_FREE_SPACE'];
$sketchsize = $_SERVER['HTTP_X_ESP8266_SKETCH_SIZE'];
$chipsize = $_SERVER['HTTP_X_ESP8266_CHIP_SIZE'];
$sdk = $_SERVER['HTTP_X_ESP8266_SDK_VERSION'];
$versionstr = $_SERVER['HTTP_X_ESP8266_VERSION'];

// split version string
list($firmname, $firmver) = explode("-", $versionstr);

// add log information
$log .= "Request by $mac running firmware $firmname version $firmver --> ";

// check firmware and version
if (array_key_exists($firmname, $latest)) {
  // firmware name exists

  // check version
  $ver = $latest[$firmname];
  $firmverf = floatval($firmver);

  if ($firmverf == $ver) {
    // already up to date
    header($_SERVER["SERVER_PROTOCOL"].' 304 Not Modified', true, 304);
    $log .= "Already up to date!\n";
    file_put_contents($logfile, $log, FILE_APPEND);
    exit();
  } elseif ($firmverf < $ver) {
    // update necessary
    $log .= "Updating to latest version: $ver!\n";
    file_put_contents($logfile, $log, FILE_APPEND);
    $filename = "$firmname-$ver.bin";
    sendFile($filename);
    exit();
  } else {
    // version is newer than server available, huh?
    header($_SERVER["SERVER_PROTOCOL"] . " 500 You version is newer than the available!.", true, 500);
    echo "Firmware is newer than the available one: $ver\n\n";
    echo "This service is only available for ESP8266 updater!\n\n";
    echo "For more information visit: https://github.com/Rup0rt/Sonoff-Smart-Switch-Amazon-Alexa\n";
    $log .= "Firmware is newer than the available one: $ver\n";
    file_put_contents($logfile, $log, FILE_APPEND);
    exit();
  }
} else {
  // report no update available
  header($_SERVER["SERVER_PROTOCOL"] . " 500 No version for this firmware.", true, 500);
  echo "Unknown firmware...\n\n";
  echo "This service is only available for ESP8266 updater!\n\n";
  echo "For more information visit: https://github.com/Rup0rt/Sonoff-Smart-Switch-Amazon-Alexa\n";
  $log .= "Unknown firmware: $firmname\n";
  file_put_contents($logfile, $log, FILE_APPEND);
  exit();
}

?>
