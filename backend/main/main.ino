// NeoPixel test program showing use of the WHITE channel for RGBW
// pixels only (won't look correct on regular RGB NeoPixel strips).

#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

#ifndef APSSID
#define APSSID "ESPap"
#define APPSK "thereisnospoon"
#endif
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define LED_PIN 12
#define LED_COUNT 97
#define BRIGHTNESS 255

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

const char *ssid = APSSID;
const char *password = APPSK;

ESP8266WebServer server(80);
DNSServer dnsServer;              // <-- Added

const byte DNS_PORT = 53;

// ------------------- Web Pages --------------------
const char page[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="UTF-8" />
		<meta name="viewport" content="width=device-width, initial-scale=1.0" />
		<title>Lamp control</title>
	</head>
	<body>
		<div id="settings-area">
			<div id="buttons">
				<button onclick="getData()">HK</button>
				<button>ADAPTIVE</button>
				<button id="btn_on" onclick="onOff()">ON</button>
			</div>
			<div class="timeline">
				<p class="upperc">Timeline</p>
				<div id="times">
					<button class="time-entry clone" time="5" onclick="handleTimeline(this)">
						<div class="time-color"></div>
						<p class="time-text small">00 min</p>
						<svg height="10px" viewBox="0 0 10 10">
							<circle cx="50%" cy="50%" r="40%" fill="white" />
						</svg>
					</button>
					<button class="time-entry-add" onclick="handleTimeline()">
						<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="white">
							<path d="M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z" />
						</svg>
					</button>
					<button id="time-entry-restart" onclick="toggleRestart()">
						<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="white">
							<path
								d="M314-115q-104-48-169-145T80-479q0-26 2.5-51t8.5-49l-46 27-40-69 191-110 110 190-70 40-54-94q-11 27-16.5 56t-5.5 60q0 97 53 176.5T354-185l-40 70Zm306-485v-80h109q-46-57-111-88.5T480-800q-55 0-104 17t-90 48l-40-70q50-35 109-55t125-20q79 0 151 29.5T760-765v-55h80v220H620ZM594 0 403-110l110-190 69 40-57 98q118-17 196.5-107T800-480q0-11-.5-20.5T797-520h81q1 10 1.5 19.5t.5 20.5q0 135-80.5 241.5T590-95l44 26-40 69Z"
							/>
						</svg>
					</button>
				</div>
			</div>
			<div class="color">
				<p class="upperc">Shown color(s) per timestamp</p>
				<div id="colors">
					<input
						type="color"
						onclick="checkForColorDeletion(event, this)"
						onchange="handleColor(this, this.value)"
						value="#fffffff"
						class="color-entry clone"
					/>
					<button class="time-entry-add" onclick="handleColor(this)">
						<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="white">
							<path d="M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z" />
						</svg>
					</button>
				</div>
			</div>
			<div class="pattern">
				<p class="upperc">Patterns the colors are shown in</p>
				<div id="patterns">
					<button class="pattern-waves" pattern="waves" onclick="handlePattern('waves')"></button>
					<!--<button class="pattern-pulse" pattern="pulse" onclick="handlePattern('pulse')"></button>-->
				</div>
			</div>
			<div id="time">
				<p class="upperc">Minutes the timestamp is shown</p>
				<div>
					<button id="minus" onclick="changeTime(-1)">
						<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="white">
							<path d="M200-440v-80h560v80H200Z" />
						</svg>
					</button>
					<input type="number" id="time-teller" onchange="changeTime(this.value)" min="0" max="10000" value="10" />
					<button id="plus" onclick="changeTime(1)">
						<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="white">
							<path d="M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z" />
						</svg>
					</button>
				</div>
			</div>
		</div>
		<button onclick="set()" id="sendBtn">update</button>
		<div id="snackbar">
			<p id="snackbarText"></p>
		</div>
	</body>
	<style>
		* {
			margin: 0;
			box-sizing: border-box;
		}
		p,
		button,
		input {
			color: white;
			font-size: var(--regular);
			font-family: Arial, Helvetica, sans-serif;
		}
		:root {
			--bg: #1c1c1c;
			--grey: #aaaaaa;
			--red: #ff4d4d;
			--radius: 3px;
			--margin: 10px;
			--border: 1px dashed var(--grey);
			--regular: 1rem;
			--small: 0.8rem;
		}
		body {
			background-color: var(--bg);
			padding: var(--margin);
			max-width: 600px;
			margin: 0 auto;
			display: flex;
			flex-direction: column;
			height: 100dvh;
		}

		#buttons {
			display: grid;
			grid-template-columns: repeat(3, 1fr);
			grid-template-rows: 1fr;
			grid-column-gap: 1px;
			grid-row-gap: 0px;
			background-color: var(--grey);
			padding: 0 1px 1px 1px;
		}
		#buttons > button {
			background-color: transparent;
			border: none;
			background-color: var(--bg);
			padding: calc(var(--margin) * 2);
			font-size: var(--small);
		}

		#settings-area {
			flex-grow: 1;
			overflow-y: scroll;
		}

		.upperc {
			text-transform: uppercase;
			font-weight: bold;
			margin-bottom: var(--margin);
		}
		.small {
			font-size: var(--small);
		}

		#buttons,
		.timeline,
		.color,
		#time,
		.pattern {
			margin-bottom: calc(var(--margin) * 4);
		}
		#times {
			border-top: 1px solid var(--grey);
			border-bottom: 1px solid var(--grey);
			padding-top: var(--margin);
		}
		#times,
		#colors {
			display: flex;
			gap: var(--margin);
			overflow-x: scroll;
		}
		#times {
			padding-bottom: calc(var(--small) + var(--margin) * 2 + 2px);
		}
		.time-entry {
			position: relative;
			border: none;
			background-color: transparent;
			padding: 0;
		}
		.time-entry.active svg {
			display: block;
		}
		.time-entry.clone,
		.color-entry.clone {
			display: none;
		}
		.time-entry > p {
			text-align: center;
			position: absolute;
			width: 100%;
			white-space: nowrap;
			overflow: hidden;
		}
		.time-entry > svg {
			display: none;
			position: absolute;
			bottom: calc((var(--small) + var(--margin) * 2) * -1);
			left: 50%;
			transform: translateX(-50%);
		}
		.time-color {
			width: calc(var(--margin) * 5);
			height: calc(var(--margin) * 15);
			background-color: red;
			border-radius: var(--radius);
		}
		#colors > * {
			width: calc(var(--margin) * 6);
			height: calc(var(--margin) * 4);
		}
		.time-entry-add {
			background-color: transparent;
			border: var(--border);
			width: calc(var(--margin) * 5);
			border-radius: var(--radius);
			flex-shrink: 0;
		}
		#time-entry-restart {
			background-color: transparent;
			border: none;
		}

		#time > div {
			display: flex;
			align-items: center;
		}
		#time > div > * {
			flex-grow: 1;
			text-align: center;
			border-radius: var(--radius);
			height: calc(var(--margin) * 4);
		}
		#time button {
			background-color: transparent;
			border: var(--border);
		}
		#time input {
			background-color: transparent;
			border: none;
		}
		.color-entry {
			appearance: none;
			background-color: transparent;
			border: none;
			padding: 0;
			border-radius: var(--radius);
			flex-shrink: 0;
		}
		#patterns {
			display: flex;
			gap: var(--margin);
		}
		#patterns > button {
			width: calc(var(--margin) * 6);
			height: calc(var(--margin) * 4);
			border-radius: var(--radius);
			border: none;
			background-color: var(--grey);
		}
		#patterns > .focus {
			border: 3px solid white !important;
		}
		.pattern-waves {
			background: linear-gradient(
				90deg,
				rgba(170, 170, 170, 1) 0%,
				rgba(97, 97, 97, 1) 20%,
				rgba(170, 170, 170, 1) 40%,
				rgba(97, 97, 97, 1) 60%,
				rgba(170, 170, 170, 1) 80%,
				rgba(97, 97, 97, 1) 100%
			);
		}
		.pattern-pulse {
		}
		#sendBtn {
			width: 100%;
			background-color: transparent;
			border: 1px solid #fff;
			border-radius: var(--radius);
			height: calc(var(--margin) * 4);
			text-transform: uppercase;
			font-weight: bold;
			margin-top: var(--margin);
		}

		div#snackbar {
			position: fixed;
			bottom: 0;
			width: calc(100% - 2 * var(--margin));
			padding: var(--margin);
			background-color: white;
			border-radius: var(--radius);
			transition: all 0.5s;
			transform: translateY(0px);
			opacity: 0;
			pointer-events: none;
		}
		p#snackbarText {
			color: black;
			font-size: var(--small);
		}
	</style>
	<script>
		let data = {
			on: false,
			restart: true,
			activeTime: 0,
			activeColor: 0,
			times: [
				{
					time: 5,
					colors: ["#ffffff"],
					pattern: "waves",
				},
			],
		};
		let hasChanges = false;
		function onOff() {
			if (data.on) {
				data.on = false;
			} else {
				data.on = true;
			}
			hasChanges = true;
			set();
			render();
		}
		function toggleRestart() {
			data.restart = !data.restart;
			hasChanges = true;
			render();
		}
		function handleTimeline(el) {
			if (el) {
				let elI = parseInt(el.getAttribute("index"));
				if (data.activeTime == elI && data.times.length > 1) {
					if (window.confirm("Delete the timestamp?")) {
						data.times.splice(elI, 1);
						if (data.activeTime == data.times.length) {
							data.activeTime -= 1;
						}
						snackbar("Timestamp deleted");
					}
				} else {
					data.activeTime = parseInt(el.getAttribute("index"));
				}
			} else {
				data.times.push({ time: 5, colors: ["#ffffff"], pattern: "solid" });
				data.activeTime = data.times.length - 1;
			}
			hasChanges = true;
			render();
		}
		function changeTime(type) {
			if (type == 1 || type == -1) {
				data.times[data.activeTime].time += 1 * type;
			} else {
				data.times[data.activeTime].time = parseInt(type);
			}
			if (data.times[data.activeTime].time < 1) data.times[data.activeTime].time = 1;
			hasChanges = true;
			render();
		}
		let lastClickedColor = -1;
		function checkForColorDeletion(event, el, value) {
			if (data.times[data.activeTime].colors.length == 1) {
				return;
			}
			let index = el.getAttribute("index");
			if (index === lastClickedColor) {
				event.preventDefault();
				if (window.confirm("Delete the color?")) {
					data.times[data.activeTime].colors.splice(index, 1);
					hasChanges = true;
					render();
					snackbar("Color deleted");
				}
			}
			lastClickedColor = index;
		}
		function handleColor(el, color) {
			console.log(data.activeColor, data.times[data.activeTime]);
			if (color) {
				data.activeColor = parseInt(el.getAttribute("index"));
				data.times[data.activeTime].colors[data.activeColor] = color;
			} else {
				data.activeColor = data.times[data.activeTime].colors.length;
				data.times[data.activeTime].colors.push("#ffffff");
			}
			hasChanges = true;
			render();
		}
		function buildStyle(colors) {
			let c = "linear-gradient(180deg, ";
			if (colors.length === 1) {
				return `linear-gradient(0deg, ${colors[0]} 0%, ${colors[0]} 100%)`;
			}
			const step = 100 / (colors.length - 1);
			colors.forEach((co, i) => {
				const pct = (step * i).toFixed(2);
				c += `${co} ${pct}%, `;
			});
			c = c.replace(/, $/, "") + ")";
			return c;
		}
		function handlePattern(pattern) {
			data.times[data.activeTime].pattern = pattern;
			hasChanges = true;
			render();
		}
		function render() {
			let parent = document.querySelector("#times");
			let toDel = document.querySelectorAll(".delete-on-rerender");
			let restartIndicator = document.querySelector("#time-entry-restart");
			if (data.restart) {
				restartIndicator.style.opacity = 1;
			} else {
				restartIndicator.style.opacity = 0.2;
			}
			document.querySelector("#btn_on").innerText = data.on ? "ON" : "OFF";
			document.querySelector("#sendBtn").style.opacity = hasChanges ? 1 : 0.5;
			toDel.forEach((el, i) => {
				el.remove();
			});
			data.times.forEach((el, i) => {
				let clone = document.querySelector(".time-entry.clone").cloneNode(true);
				clone.classList.remove("clone");
				clone.querySelector(".time-text").innerText = el.time + " min";
				let cStyler = clone.querySelector(".time-color").style;
				cStyler.background = buildStyle(el.colors);
				cStyler.width = `calc(${el.time} * var(--margin))`;
				clone.setAttribute("index", i);
				if (data.activeTime == i) {
					clone.classList.add("active");
					document.querySelector("#time-teller").value = el.time;
					let cParent = document.querySelector("#colors");
					el.colors.forEach((col, cI) => {
						let cClone = document.querySelector(".color-entry.clone").cloneNode(true);
						cClone.classList.remove("clone");
						cClone.value = col;
						cClone.classList.add("delete-on-rerender");
						cClone.setAttribute("index", cI);
						cParent.insertBefore(cClone, cParent.childNodes[cParent.childNodes.length - 2]);
					});
					document.querySelector(".pattern").style.display = el.colors.length == 1 ? "none" : "block";
					document.querySelectorAll("[pattern]").forEach((pat) => {
						pat.classList.remove("focus");
						if (pat.getAttribute("pattern") == el.pattern) {
							pat.classList.add("focus");
						}
					});
				}
				clone.classList.add("delete-on-rerender");
				parent.insertBefore(clone, parent.childNodes[parent.childNodes.length - 4]);
			});
		}
		render();
		async function set() {
			if (!hasChanges) {
				return;
			}
			try {
				let res = await fetch("/set", {
					method: "POST",
					body: JSON.stringify(data),
				});
				if (res.ok) {
					hasChanges = false;
					render();
					snackbar("Updated");
				} else {
					snackbar("Error while updating, reload and try again.", true);
				}
			} catch (e) {
				snackbar("Error", true);
				console.error(e);
			}
		}
		async function getData() {
			try {
				let res = await fetch("/state");
				if (res.ok) {
					let serialized = await res.json();
					console.log(serialized);
					if (serialized) {
						data = serialized;
						render();
					}
				} else {
					console.error("Couldn't get state", res);
					snackbar("Couldn't get state, restart lamp and try again", true);
				}
			} catch (e) {
				console.error(e);
				snackbar("Error", true);
			}
		}
		getData();

		function snackbar(msg, isError = false) {
			let elem = document.querySelector("#snackbar");
			let txt = document.querySelector("#snackbarText");
			txt.innerText = msg;

			elem.style.backgroundColor = isError ? "var(--red)" : "white";

			elem.style.transform = "translateY(calc(var(--margin) * -1))";
			elem.style.opacity = 1;
			elem.style.pointerEvents = "all";

			setTimeout(() => {
				elem.style.transform = "translateY(0px)";
				elem.style.opacity = 0;
				elem.style.pointerEvents = "none";
			}, 1500);
		}
	</script>
</html>
  )rawliteral";
void handleRoot() {
  server.send(200, "text/html", page);
}

// ------------------- Light Class -------------------

class Light {
	private:
		bool hasChanges = true;
		JsonDocument data;
		unsigned long timeSinceDataWasSet = 0;
		unsigned long timeCurrentTimelineIsStarted = 0;
		int currentTimeIndex = 0;

		const int led_pattern[10][12] = {
			{-1, -1, 15, 14, 13, 12, 11, 10, 9, 8, 7, -1},
			{-1, -1, 44, 43, 42, 41, 40, 39, 38, 37, 36, -1},
			{16, -1, 96, 95, 94, 93, 92, 91, 90, 89, -1, 6},
			{17, 45, 81, 82, 83, 84, 85, 86, 87, 88, 35, 5},
			{-1, 46, 80, 79, 78, 77, 76, 75, 74, 73, 34, -1},
			{18, 47, 65, 66, 67, 68, 69, 70, 71, 72, 33, 4},
			{19, 48, 64, 63, 62, 61, 60, 59, 58, 57, 32, 3},
			{20, -1, 49, 50, 51, 52, 53, 54, 55, 56, -1, 2},
			{-1, -1, 23, 24, 25, 26, 27, 28, 29, 30, 31, -1},
			{-1, 21, 22, -1, -1, -1, -1, -1, -1, 0, 1, -1}
		};
  public:
		void setData(JsonDocument givenData){
			data = givenData;
			hasChanges = true;
			timeSinceDataWasSet = millis();
			timeCurrentTimelineIsStarted = millis();
			currentTimeIndex = 0;
		}

		// Sets the whole lamp to display a color color
    void setSolid(const char color[]){
      int r, g, b;
      hexToRGB(color, r, g, b);
      strip.fill(strip.Color(r, g, b));
    }
		void setPattern(const char pattern[]){
			for(int i = 0; i < 10; i++){
				strip.setPixelColor(led_pattern[i][4], strip.Color(255, 0, 0));
			}
		}
		void update(unsigned long currentUpdateMs){
			if(data.isNull()){
				return;
			}

			JsonObject currentTimeObject = data["times"][currentTimeIndex];

			int secondsTimeIsDisplayedOnTimeline = currentTimeObject["time"];
			if(currentTimeIndex > 0){
				for(int i = 0; i < currentTimeIndex; i++){
					int accumulatedTime = data["times"][i]["time"] | 0;
					secondsTimeIsDisplayedOnTimeline += accumulatedTime;
				}
			}

			const char* currentColor = currentTimeObject["colors"][0] | "";

			if(currentUpdateMs >= timeCurrentTimelineIsStarted + (secondsTimeIsDisplayedOnTimeline * 1000)){
				bool restartAfterTimelineEnd = data["restart"];
				if(currentTimeIndex < data["times"].size() && data["times"].size() > 1){
					currentTimeIndex += 1;
				}else{
					if(restartAfterTimelineEnd){
						currentTimeIndex = 0;
						timeCurrentTimelineIsStarted = millis();
					}else{
						data["on"] = false;
					}
				}
			}

			bool on = data["on"] | false;
			if(on == false){
				setSolid("#000000");
				strip.show();
				return;
			}
			setSolid(currentColor);

			strip.show();
		}
    void hexToRGB(const String &hex, int &r, int &g, int &b) {
      String h = hex;
      if (h.startsWith("#")) h = h.substring(1);
      // If shorthand #abc → #aabbcc
      if (h.length() == 3) {
        h = String(h[0]) + String(h[0]) +
            String(h[1]) + String(h[1]) +
            String(h[2]) + String(h[2]);
      }
      // Now h should be 6 chars
      if (h.length() != 6) return;
      r = strtol(h.substring(0, 2).c_str(), NULL, 16);
      g = strtol(h.substring(2, 4).c_str(), NULL, 16);
      b = strtol(h.substring(4, 6).c_str(), NULL, 16);
    }

    void setBrightness(const int brightness){
      strip.setBrightness(brightness);
    }

		JsonDocument getData(){
			return data;
		}

    void init(){
      strip.begin();
    }
};

Light light;

//int waves_pattern[][] = {{2, 3, 4, 5, 6}, {1, 32}};

// ------------------- Setup -------------------------
void setup() {
  #if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
    clock_prescale_set(clock_div_1);
  #endif

  light.init();
  light.setBrightness(BRIGHTNESS);

  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.println("Configuring access point...");

  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // ============= DNS Hijack (Captive Portal) =============
  // This redirects ALL domains to the ESP8266 IP
  dnsServer.start(DNS_PORT, "*", myIP);
  Serial.println("DNS server started. All domains -> ESP8266");

  // ============= HTTP Routes =============================
  server.on("/", handleRoot);

  server.on("/set", HTTP_POST, []() {
    String body = server.arg("plain");
    Serial.println("POST body: " + body);
    JsonDocument doc;
    deserializeJson(doc, body);

		light.setData(doc);
    
    server.send(200, "text/plain", "OK");
  });

	server.on("/state", HTTP_GET, []() {
		JsonDocument data;

		data = light.getData();

		String response;
		serializeJson(data, response);

		server.send(200, "application/json", response);
	});

  server.begin();
  Serial.println("HTTP server started");
}

// ------------------- Loop -----------------------------
unsigned long previousMillis = 0;
const unsigned long interval = 50;  // ms between LED updates

void loop() {
	unsigned long currentMillis = millis();

	if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    light.update(currentMillis);
  }

  dnsServer.processNextRequest();   // <-- Required for DNS spoofing
  server.handleClient();
}
