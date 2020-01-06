<?php
/*
 * Project: Remote Mail Notifier (and GPS Tracker)
 * Author: Zak Kemble, contact@zakkemble.net
 * Copyright: (C) 2020 by Zak Kemble
 * License: 
 * Web: https://blog.zakkemble.net/remote-mail-notifier-and-gps-tracker/
 */

	// https://apps.timwhitlock.info/emoji/tables/unicode

	// https://stackoverflow.com/questions/9241800/merging-two-complex-objects-in-php
	function my_merge($arr1, $arr2)
	{
		$keys = array_keys($arr2);
		foreach($keys as $key)
		{
			if(
				isset($arr1[$key]) &&
				is_array($arr1[$key]) &&
				is_array($arr2[$key])
			)
				$arr1[$key] = my_merge($arr1[$key], $arr2[$key]);
			else
				$arr1[$key] = $arr2[$key];
		}
		return $arr1;
	}

	// Debugging
	// Read test.json instead of the JSON POST data from the client
	$jsonSourceDebug = false;

	// Log JSON data to log/ directory (make sure it exists and writable)
	$logJsonData = false;

	// Speed things up a bit by spawning curl processes to send the Telegram API requests
	// However, we can't tell if the request was successful with this enabled
	// Most web hosts probably won't allow spawning processes
	$fastResponse = false;

	$TG_TOKEN = '000000000:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'; // Telegram bot token
	$TG_CHATID = '-0000000000000'; // Group chat ID

	date_default_timezone_set('Etc/UTC');

	// Blank out headers to reduce response size
	header('Date: ');
	header('Content-Type: ');
	header('Server: ');

	$jsonIn = file_get_contents($jsonSourceDebug ? 'test.json' : 'php://input');
	$jsonLength = strlen($jsonIn);
	if(!$jsonLength || $jsonLength > 4096)
		die('{"result":"error"}');

	if($logJsonData)
	{
		$fileName = gmdate('ymd-His', $_SERVER['REQUEST_TIME']); // TODO what if 2 requests in the same second?
		file_put_contents('log/' . $fileName . '.json', $jsonIn); // TODO use JSON_PRETTY_PRINT
	}
	
	$jsonDefaultIn = file_get_contents('default.json');
	
	$obj = null;
	if(strlen($jsonDefaultIn))
	{
		$json1 = json_decode($jsonDefaultIn, true);
		if($json1 === NULL || json_last_error() != JSON_ERROR_NONE)
		{
			die('{"result":"error"}');
		}
		
		$json2 = json_decode($jsonIn, true);
		if($json2 === NULL || json_last_error() != JSON_ERROR_NONE)
		{
			die('{"result":"error"}');
		}

		$res = my_merge($json1, $json2);
		$obj = json_decode(json_encode($res));
	}

	if($obj === null)
		die('{"result":"error"}');

	$msgData = [];
	if($obj->reasons->newmail)
	{
		$msgData[] = [
			"%s *You have mail!*\n",
			"\xF0\x9F\x93\xA8"
		];
	}
	if($obj->reasons->endcharge)
	{
		$msgData[] = [
			"%s *Charge complete!*\n",
			"\xF0\x9F\x94\x8B"
		];
	}
	if($obj->reasons->trackmode)
	{
		$msgData[] = [
			"%s *Bug/tracking mode enabled!*\n",
			"\xF0\x9F\x93\xA1"
		];
	}
	if($obj->reasons->switchstuck)
	{
		$msgData[] = [
			"%s *Switch stuck!*\n",
			"\xE2\x9A\xA0"
		];
	}
	$msgData[] = [
		"\n",
	];
	$msgData[] = [ // Battery level
		"%s %umV (%u%%) (VLM: %u) %s\n",
		"\xE2\x9A\xA1",
		$obj->battery->voltage,
		$obj->battery->percent,
		$obj->battery->vlm,
		($obj->battery->voltage > 3600 && !$obj->battery->vlm) ? "\xE2\x9C\x85" : "\xE2\x9D\x8C"
	];
	$msgData[] = [ // Signal level
		"%s %u/31\n",
		"\xF0\x9F\x93\xB6",
		$obj->network->signal
	];
/*
	$msgData[] = [
		"%s %s\n",
		"\xF0\x9F\x8C\x8D",
		$obj->network->ip
	];
	$msgData[] = [
		"%s %s\n",
		"\xF0\x9F\x93\xB1",
		$obj->network->imei
	];
	$msgData[] = [
		"%s %s\n",
		"\xF0\x9F\x92\xB3",
		$obj->network->iccid
	];
*/
	$msgData[] = [ // BME280 sensor info
		"%s %.2f\xC2\xB0C / %.2f%% / %.2f hPa\n",
		"\xE2\x9B\x85",
		$obj->environment->temperature,
		$obj->environment->humidity,
		$obj->environment->pressure
	];
	if($obj->reasons->newmail || $obj->reasons->endcharge || $obj->reasons->switchstuck)
	{
		$msgData[] = [
			"Success: *%u*\n",
			$obj->counts->success
		];
		$msgData[] = [
			"Failure: *%u*\n",
			$obj->counts->failure
		];
		$msgData[] = [
			"Timeout: *%u*\n",
			$obj->counts->timeout
		];
	}
	if($obj->balance->state != 0) // PAYG balance
	{
		$msgData[] = [
			"%s\n",
			strlen($obj->balance->message) ? $obj->balance->message : '_Unknown balance_'
		];
	}
	if($obj->reasons->trackmode)
	{
		$fixTypes = [
			'?',
			'None',
			'2D',
			'3D',
		];
		$fixQualities = [
			'None',
			'3D',
			'3D/DGPS',
			'PPS',
			'Real Time Kinematic',
			'Float RTK',
			'Estimated (dead reckoning)',
			'Manual input',
			'Simulation',
		];
		$fixQuality = $fixQualities[0];
		$isFixed = ($obj->track->gps->fix > $obj->track->bds->fix) ? $obj->track->gps->fix : $obj->track->bds->fix;
		if($isFixed == 2) // 2D
			$fixQuality = '2D';
		else if($isFixed == 3) // 3D
		{
			//if($obj->track->quality == 1)
			//	$fixQuality = '3D';
			//else if($obj->track->quality == 2)
			//	$fixQuality = '3D/DGPS';
			if(array_key_exists($obj->track->quality, $fixQualities))
				$fixQuality = $fixQualities[$obj->track->quality];
			else
				$fixQuality = 'Unknown';
		}
		
		$compass = '?';
		if($obj->track->course >= 337.5)
			$compass = 'N';
		else if($obj->track->course >= 292.5)
			$compass = 'NW';
		else if($obj->track->course >= 247.5)
			$compass = 'W';
		else if($obj->track->course >= 202.5)
			$compass = 'SW';
		else if($obj->track->course >= 157.5)
			$compass = 'S';
		else if($obj->track->course >= 112.5)
			$compass = 'SE';
		else if($obj->track->course >= 67.5)
			$compass = 'E';
		else if($obj->track->course >= 22.5)
			$compass = 'NE';
		else if($obj->track->course >= 0)
			$compass = 'N';

		$msgData[] = [
			"\n%s *TRACKING* %s\n",
			"\xF0\x9F\x93\xA1",
			"\xF0\x9F\x93\xA1"
		];
		$msgData[] = [
			"Fix: GPS: *%s* / BDS: *%s*\n",
			$fixTypes[$obj->track->gps->fix],
			$fixTypes[$obj->track->bds->fix]
		];
		$msgData[] = [
			"Quality: *%s* %s\n",
			$fixQuality,
			($fixQuality == $fixQualities[0] || $fixQuality == 'Unknown') ? "\xE2\x9D\x8C" : "\xE2\x9C\x85"
		];
		$msgData[] = [
			"Speed: *%.1f MPH*\n",
			$obj->track->speed / 1.609 // KPH to MPH
		];
		$msgData[] = [
			"Course: *%.2f\xC2\xB0 (%s)*\n",
			$obj->track->course,
			$compass
		];
		$msgData[] = [
			"Altitude: *%.2f m*\n",
			$obj->track->altitude
		];
		$msgData[] = [ // Number of satellites in view and tracking
			"Satellites: *%u/%u* -> GPS: *%u%s/%u* / BDS: *%u%s/%u*\n",
			$obj->track->sattrack,
			$obj->track->gps->sattotal + $obj->track->bds->sattotal,
			$obj->track->gps->sattrack,
			($obj->track->gps->sattrack >= 12) ? '+' : '',
			$obj->track->gps->sattotal,
			$obj->track->bds->sattrack,
			($obj->track->bds->sattrack >= 12) ? '+' : '',
			$obj->track->bds->sattotal,
		];
		$msgData[] = [ // GPS date/time
			"%02u/%02u/%02u %02u:%02u:%02u.%03u\n",
			$obj->track->date->d,
			$obj->track->date->m,
			$obj->track->date->y,
			$obj->track->time->h,
			$obj->track->time->m,
			$obj->track->time->s,
			$obj->track->time->ms,
		];
		$msgData[] = [ // Google maps link
			"https://www.google.com/maps/@%F,%F,%.2Fz\n",
			$obj->track->latitude,
			$obj->track->longitude,
			15 // max 21
		];
	}

	// Create final string
	$fmt = '';
	$args = [];
	for($i=0;$i<count($msgData);++$i)
	{
		$fmt .= $msgData[$i][0];
		for($x=1;$x<count($msgData[$i]);++$x)
			$args[] = $msgData[$i][$x];
	}
	$tgMsg = vsprintf($fmt, $args);

	// The main telegram message
	$tgUrl = 'https://api.telegram.org/bot' . $TG_TOKEN . '/sendMessage';
	$tgJsonData = array(
		'chat_id' => $TG_CHATID,
		'parse_mode' => 'markdown',
		'disable_web_page_preview' => true,
		'text' => $tgMsg
	);
	$tgJsonDataEncoded = json_encode($tgJsonData);
	
	// Telegram location message
	$tgLocationReq = '';
	if($obj->reasons->trackmode)
	{
		$tgLocationReq = sprintf(
			'https://api.telegram.org/bot%s/sendlocation?chat_id=%s&latitude=%F&longitude=%F',
			$TG_TOKEN,
			$TG_CHATID,
			$obj->track->latitude,
			$obj->track->longitude
		);
	}

	// NOTE:
	// When in tracking mode we want to make sure the location is sent after the main message
	// This means we can't use curls multiple request feature

	if($fastResponse)
	{
		if($obj->reasons->trackmode)
			exec("curl -m 5 -H 'Content-Type: application/json' -d " . escapeshellarg($tgJsonDataEncoded) . " '{$tgUrl}' >/dev/null && curl -m 5 '{$tgLocationReq}' >/dev/null &");
		else
			exec("curl -m 5 -H 'Content-Type: application/json' -d " . escapeshellarg($tgJsonDataEncoded) . " '{$tgUrl}' >/dev/null &");
	}
	else
	{
		$ch = curl_init($tgUrl);
		curl_setopt($ch, CURLOPT_POST, 1);
		curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
		curl_setopt($ch, CURLOPT_POSTFIELDS, $tgJsonDataEncoded);
		curl_setopt($ch, CURLOPT_HTTPHEADER, array('Content-Type: application/json')); 
		$tgResponse = curl_exec($ch);
		//echo $tgResponse;
		
		if($obj->reasons->trackmode)
		{
			$tgResponse = file_get_contents($tgLocationReq);
			//echo $tgResponse;
		}
	}

	echo '{"result":"ok"}';
