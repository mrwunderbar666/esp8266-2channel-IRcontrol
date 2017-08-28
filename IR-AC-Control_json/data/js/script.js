// JavaScript Document
"use strict";
updateMeasures();
updateConfig();
// auto-update values 
setInterval(updateMeasures, 60000); //60000 MS == 1 minute
// Buttons for IR Channel 0
$('#ch0_tempdown').click(function () {
	setIR('0', '2170966446');
});
$('#ch0_tempup').click(function () {
	setIR('0', '2170986846');
});
$('#ch0_fan').click(function () {
	setIR('0', '2170984806');
});
$('#ch0_swing').click(function () {
	setIR('0', '2170995006');
});
$('#ch0_mode').click(function () {
	setIR('0', '2171001126');
});
$('#ch0_power').click(function () {
	setIR('0', '2170978686');
});
$('#ch0_timer').click(function () {
	setIR('0', '2171009286');
});

//Buttons for IR Channel 1

$('#ch1_tempdown').click(function () {
	setIR('1', '2170966446');
});
$('#ch1_tempup').click(function () {
	setIR('1', '2170986846');
});
$('#ch1_fan').click(function () {
	setIR('1', '2170984806');
});
$('#ch1_swing').click(function () {
	setIR('1', '2170995006');
});
$('#ch1_mode').click(function () {
	setIR('1', '2171001126');
});
$('#ch1_power').click(function () {
	setIR('1', '2170978686');
});
$('#ch1_timer').click(function () {
	setIR('1', '2171009286');
});

// Buttons for configuration
//$('#config_tab').click(function(){ updateConfig(); 	});
$('#enable_ota').click(function () {
	console.log("Enable OTA Button Pressed");
	setOTA('1');
});
$('#disable_ota').click(function () {
	console.log("Disable Button Pressed");
	setOTA('0');
});
$('#refresh_config').click(function () {
	console.log("Refresh Config Button Pressed");
	updateConfig();
});
$('#refresh_spiffs').click(function () {
	console.log("Refresh SPIFFS Button Pressed");
	updateSPIFFS();
});

function setIR(channel, code) {
	$.post("ir?channel=" + channel + "&code=" + code).done(function (data) {
		console.log("Return setIR " + JSON.stringify(data));
		var return_code = "#" + code + "_state";
		console.log(data);
		if (data.success === "1" || data.success === 1) {
			$(return_code).html(data.code);
		} else {
			$(return_code).html('!');
		}
	}).fail(function (err) {
		console.log("err setIR " + JSON.stringify(err));
	});
}

function updateMeasures() {
	$.getJSON('/measures.json', function (data) {
		//console.log("Mesures envoyees : " + JSON.stringify(data) + "|" + data.t + "|" + data.h + "|" + data.pa) ;
		$('#temperature').html(data.temperature + " &deg;C");
		$('#humidity').html(data.humidity + " %");
		$('#pressure').html(data.pressure + " mBar");
	}).fail(function (err) {
		console.log("err getJSON measures.json " + JSON.stringify(err));
	});
}

function updateConfig() {
	$.getJSON('/current_config.json', function (data) {
		console.log("Current Config : " + JSON.stringify(data));
		$('#device-id').html(data.device_name);
		$('#device-topic').html(data.device_topic);
		$('#device-ip').html(data.ip_address);
		if (data.update_mode === "1" || data.update_mode === 1) {
			$('#enable_ota').prop('disabled', true);
			$('#disable_ota').prop('disabled', false);
			$("#ota_alert").show();
		} else {
			$('#enable_ota').prop('disabled', false);
			$('#disable_ota').prop('disabled', true);
			$("#ota_alert").hide();
		}
	}).fail(function (err) {
		console.log("err getJSON current_config.json " + JSON.stringify(err));
	});
}

function updateSPIFFS() {
	$.getJSON('/spiffs.json', function (data) {
		console.log("Current SPIFFS : " + JSON.stringify(data));
		$('#used_bytes').html(data.used_bytes);
		$('#total_bytes').html(data.total_bytes);
		$('#block_size').html(data.block_size);
		$('#page_size').html(data.page_size);
		$('#max_open_files').html(data.max_open_files);
		$('#max_path_length').html(data.max_path_length);		
	}).fail(function (err) {
		console.log("err getJSON spiffs.json " + JSON.stringify(err));
	});
}

function setOTA(flag) {
	console.log("OTA Button: " + flag);
	$.post("config?ota=" + flag).done(updateConfig());
}

/*
				function(data){
          console.log("Return setOTA " + JSON.stringify(data)); 
          var return_config = "#" + flag + "_state";
          console.log(data);
          if ( data.success === "1" | data.success === 1 ) {
              $(return_config).html(data.device_name);
			  if (data.update_mode === "1" | data.update_mode === 1) {
				  $('#enable_ota').prop('disabled', true);
				  $('#disable_ota').prop('disabled', false);
				  $("#ota_alert").show();
			  }
			  if (data.update_mode === "0" | data.update_mode === 0) {
				  $('#enable_ota').prop('disabled', false);
				  $('#disable_ota').prop('disabled', true);
				  $("#ota_alert").hide();
			  }  
          } else {
            $(return_config).html('!');
          }      
        }).fail(function(err){
          console.log("err setIR " + JSON.stringify(err));
        });		
		}
		*/