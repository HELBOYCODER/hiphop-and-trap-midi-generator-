package com.hhmg.studio

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat

private val Amber = Color(0xFFFFB020)
private val Teal = Color(0xFF39C5BB)
private val Ink = Color(0xFF111215)
private val Panel = Color(0xFF1A1C21)

class MainActivity : ComponentActivity() {
    private val vm: MainViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = darkColorScheme(primary = Amber, onPrimary = Ink, secondary = Teal, background = Ink, surface = Panel, onSurface = Color(0xFFE9E6DF))) {
                Screen(vm)
            }
        }
    }
}

@Composable
private fun Screen(vm: MainViewModel) {
    val ctx = LocalContext.current
    val micPermission = rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        if (granted) vm.startRecording() else Toast.makeText(ctx, "برای ضبط، دسترسی میکروفون لازم است", Toast.LENGTH_LONG).show()
    }
    val pickAudio = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) vm.importAudio(uri) }
    val pickMidi = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris -> if (uris.isNotEmpty()) vm.addMidiFiles(uris) }
    fun toggleRecord() {
        if (vm.recording) vm.stopRecording()
        else if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) vm.startRecording()
        else micPermission.launch(Manifest.permission.RECORD_AUDIO)
    }

    Column(Modifier.fillMaxSize().background(Ink).statusBarsPadding().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)) {

        Text("HipHop MIDI Studio", color = Amber, fontSize = 26.sp, fontWeight = FontWeight.Bold)
        Text("صدا را بخوان، MIDI بگیر، یا ایده‌ی مشابه بساز", color = Color(0xFF9A9CA4), fontSize = 13.sp)

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("صدا", "جنریتور", "شعر").forEachIndexed { i, t -> FilterChip(selected = vm.tab == i, onClick = { vm.tab = i }, label = { Text(t) }) }
        }

        if (vm.tab == 1) GeneratorCard(vm)
        if (vm.tab == 2) LyricsCard(vm)

        // ---- record
        if (vm.tab == 0) Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Button(onClick = { toggleRecord() }, enabled = !vm.busy, modifier = Modifier.fillMaxWidth().height(56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = if (vm.recording) Color(0xFFE0526B) else Amber, contentColor = Ink)) {
                    Text(if (vm.recording) "■  توقف و تحلیل" else "●  ضبط صدا", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                }
                if (vm.recording) {
                    LinearProgressIndicator(progress = { vm.level.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth(), color = Teal)
                    Text("%.1f ثانیه".format(vm.recordSeconds), color = Color(0xFF9A9CA4), fontSize = 12.sp)
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                    OutlinedButton(onClick = { pickAudio.launch(arrayOf("audio/*")) }, enabled = !vm.busy && !vm.recording, modifier = Modifier.weight(1f)) { Text("ایمپورت فایل صوتی") }
                    OutlinedButton(onClick = { pickMidi.launch(arrayOf("audio/midi", "audio/x-midi", "audio/mid", "application/octet-stream", "*/*")) }, enabled = !vm.busy, modifier = Modifier.weight(1f)) { Text("MIDIهای من") }
                }
                Text(vm.status, color = Color(0xFFE9E6DF), fontSize = 14.sp)
            }
        }

        val clip = vm.clip
        val tr = vm.transcription
        if (vm.tab != 0 && clip != null) Text(vm.status, color = Color(0xFFE9E6DF), fontSize = 13.sp)
        if (clip != null && tr != null && vm.tab == 0) {
            // ---- options
            Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("نوع خروجی", color = Amber, fontWeight = FontWeight.SemiBold)
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Mode.values().forEach { m -> FilterChip(selected = vm.mode == m, onClick = { vm.mode = m; vm.process() }, label = { Text(m.label) }) }
                    }
                    Text("سبک", color = Amber, fontWeight = FontWeight.SemiBold)
                    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        listOf("" to "خودکار", "rnb" to "R&B", "trap" to "Trap", "hiphop" to "Hip-Hop").forEach { (k, label) ->
                            FilterChip(selected = vm.style == k, onClick = { vm.style = k; vm.process() }, label = { Text(label) })
                        }
                    }
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Switch(checked = vm.bass, onCheckedChange = { vm.bass = it; vm.process() })
                        Text("خط ۸۰۸", color = Color(0xFFE9E6DF))
                    }
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        OutlinedTextField(value = vm.bpmText, onValueChange = { vm.bpmText = it.filter { c -> c.isDigit() }.take(3) }, singleLine = true,
                            label = { Text("BPM (خالی = خودکار)") }, keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number), modifier = Modifier.weight(1f))
                        OutlinedButton(onClick = { vm.stopPlayback(); vm.applyBpmAndReanalyze() }) { Text("اعمال") }
                    }
                    Button(onClick = { vm.variation() }, enabled = !vm.busy, modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.buttonColors(containerColor = Teal, contentColor = Ink)) { Text("ایده‌ی دیگر", fontWeight = FontWeight.Bold) }
                }
            }

        }
        if (clip != null) {
            // ---- piano roll + transport
            Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(if (vm.clipFromVoice) "پیانو رول  ·  خط فیروزه‌ای = فرکانس صدای شما" else "پیانو رول", color = Amber, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    PianoRoll(clip, if (vm.clipFromVoice && tr != null) tr.contour else emptyList(), if (vm.clipFromVoice && tr != null) tr.startOffsetSec else 0.0, vm.playTicks, vm.playing)
                    val active = clip.notes.filter { it.channel == 1 && vm.playTicks >= it.start && vm.playTicks < it.start + it.length }.firstOrNull()
                    Text(if (vm.playing && active != null) "${noteName(active.pitch)}  ·  ${hzText(active.pitch)}" else "${clip.notes.size} نت  ·  ${clip.bpm} BPM  ·  ${clip.bars} میزان",
                        color = Color(0xFF9A9CA4), fontSize = 13.sp)
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                        Button(onClick = { vm.togglePlay() }, modifier = Modifier.weight(1f)) { Text(if (vm.playing) "■ توقف" else "▶ پخش") }
                        OutlinedButton(onClick = { Toast.makeText(ctx, vm.save(), Toast.LENGTH_LONG).show() }, modifier = Modifier.weight(1f)) { Text("ذخیره MIDI") }
                        OutlinedButton(onClick = { vm.share() }, modifier = Modifier.weight(1f)) { Text("اشتراک") }
                    }
                }
            }

            if (vm.analysisText.isNotEmpty()) {
                Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
                    Column(Modifier.padding(16.dp)) {
                        Text("تشخیص سبک", color = Amber, fontWeight = FontWeight.SemiBold)
                        Spacer(Modifier.height(6.dp))
                        Text(vm.analysisText, color = Color(0xFFE9E6DF), fontSize = 13.sp, lineHeight = 20.sp)
                    }
                }
            }
        }

        // ---- AI
        Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("هوش مصنوعی (نیاز به اینترنت)", color = Amber, fontWeight = FontWeight.SemiBold)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilterChip(selected = vm.provider == "gemini", onClick = { vm.provider = "gemini"; if (vm.model.isBlank() || !vm.model.startsWith("gemini")) vm.model = "gemini-2.5-flash" }, label = { Text("Gemini") })
                    FilterChip(selected = vm.provider == "openai", onClick = { vm.provider = "openai"; if (vm.model.startsWith("gemini")) vm.model = "gpt-4o-mini" }, label = { Text("OpenAI-compatible") })
                }
                if (vm.provider == "openai") OutlinedTextField(value = vm.endpoint, onValueChange = { vm.endpoint = it }, singleLine = true,
                    label = { Text("Endpoint (…/v1) — OpenAI, OpenRouter, Ollama, LM Studio") }, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(value = vm.apiKey, onValueChange = { vm.apiKey = it }, singleLine = true, label = { Text(if (vm.provider == "gemini") "Gemini API key" else "API key (برای مدل محلی خالی بماند)") },
                    visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                OutlinedTextField(value = vm.model, onValueChange = { vm.model = it }, singleLine = true, label = { Text("مدل") }, modifier = Modifier.fillMaxWidth())
                McpCard(vm)
                var prompt by remember { mutableStateOf("") }
                OutlinedTextField(value = prompt, onValueChange = { prompt = it }, label = { Text("مثلاً: مثل این ولی تاریک‌تر، ترپ، با ۸۰۸ سنگین") }, modifier = Modifier.fillMaxWidth(), minLines = 2)
                Button(onClick = { vm.askAi(prompt) }, enabled = !vm.aiBusy, modifier = Modifier.fillMaxWidth(),
                    colors = ButtonDefaults.buttonColors(containerColor = Teal, contentColor = Ink)) { Text(if (vm.aiBusy) "..." else "بساز با AI", fontWeight = FontWeight.Bold) }
                if (vm.aiMessage.isNotEmpty()) Text(vm.aiMessage, color = Color(0xFFE9E6DF), fontSize = 13.sp, lineHeight = 20.sp)
            }
        }
        Text(vm.libraryInfo, color = Color(0xFF6B6D75), fontSize = 11.sp)
        Spacer(Modifier.height(24.dp))
    }
}


@Composable
private fun Chip(label: String, on: Boolean, click: () -> Unit) = FilterChip(selected = on, onClick = click, label = { Text(label) })

@Composable
private fun Slider01(label: String, v: Float, onChange: (Float) -> Unit) {
    Column {
        Text("$label  ${"%.0f".format(v * 100)}%", color = Color(0xFFE9E6DF), fontSize = 12.sp)
        androidx.compose.material3.Slider(value = v, onValueChange = onChange, valueRange = 0f..1f)
    }
}

/** Same controls as the VST3 generator: key, scale lock, style mix, complexity, humanize, swing, parts, seed, presets. */
@Composable
private fun GeneratorCard(vm: MainViewModel) {
    Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("پریست‌ها", color = Amber, fontWeight = FontWeight.SemiBold)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf("Dark Trap", "Smooth R&B", "Boom Bap", "Drill", "Soul Hip-Hop").forEachIndexed { i, n -> OutlinedButton(onClick = { vm.applyPreset(i) }, enabled = !vm.busy) { Text(n) } }
            }
            Text("کلید", color = Amber, fontWeight = FontWeight.SemiBold)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                (0..11).forEach { Chip(vm.noteLabel(it), vm.gRoot == it) { vm.gRoot = it } }
            }
            Text("گام (قفل: همه نت‌ها داخل گام می‌مانند)", color = Amber, fontWeight = FontWeight.SemiBold)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                vm.scales.forEach { (id, label) -> Chip(label, vm.gScale == id) { vm.gScale = id } }
            }
            Text("میزان: ${vm.gBars}   BPM: ${vm.gBpm}", color = Color(0xFFE9E6DF), fontSize = 12.sp)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) { listOf(2, 4, 8, 16).forEach { Chip("$it", vm.gBars == it) { vm.gBars = it } } }
            androidx.compose.material3.Slider(value = vm.gBpm.toFloat(), onValueChange = { vm.gBpm = it.toInt() }, valueRange = 60f..180f)
            Slider01("R&B", vm.gRnb) { vm.gRnb = it }; Slider01("Trap", vm.gTrap) { vm.gTrap = it }; Slider01("Hip-Hop", vm.gHip) { vm.gHip = it }
            Slider01("پیچیدگی", vm.gComplexity) { vm.gComplexity = it }; Slider01("Humanize", vm.gHumanize) { vm.gHumanize = it }
            Slider01("Swing", vm.gSwing) { vm.gSwing = it }; Slider01("تنوع ولوم", vm.gVelVar) { vm.gVelVar = it }
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Switch(checked = vm.gChords, onCheckedChange = { vm.gChords = it }); Text("آکورد", color = Color(0xFFE9E6DF))
                Switch(checked = vm.gMelody, onCheckedChange = { vm.gMelody = it }); Text("ملودی", color = Color(0xFFE9E6DF))
                Switch(checked = vm.gBass, onCheckedChange = { vm.gBass = it }); Text("۸۰۸", color = Color(0xFFE9E6DF))
            }
            Text("اکتاو ملودی: ${vm.gOctave}", color = Color(0xFFE9E6DF), fontSize = 12.sp)
            androidx.compose.material3.Slider(value = vm.gOctave.toFloat(), onValueChange = { vm.gOctave = it.toInt() }, valueRange = 3f..7f, steps = 3)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                Button(onClick = { vm.generate(true) }, enabled = !vm.busy, modifier = Modifier.weight(1f)) { Text("GENERATE", fontWeight = FontWeight.Bold) }
                OutlinedButton(onClick = { vm.generate(false) }, enabled = !vm.busy, modifier = Modifier.weight(1f)) { Text("seed ${vm.gSeed}") }
            }
        }
    }
}

@Composable
private fun LyricsCard(vm: MainViewModel) {
    Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("شعر / متن ترانه → نت", color = Amber, fontWeight = FontWeight.SemiBold)
            Text("هر خط یک عبارت می‌شود؛ تعداد هجاها ریتم را تعیین می‌کند. هر زبانی (فارسی هم).", color = Color(0xFF9A9CA4), fontSize = 12.sp)
            OutlinedTextField(value = vm.lyricsText, onValueChange = { vm.lyricsText = it }, minLines = 5, modifier = Modifier.fillMaxWidth(), label = { Text("شعر یا متن ترانه") })
            Text("سبک و گام از تب «جنریتور» می‌آید.", color = Color(0xFF9A9CA4), fontSize = 12.sp)
            Button(onClick = { vm.makeFromLyrics() }, enabled = !vm.busy, modifier = Modifier.fillMaxWidth()) { Text("ساخت ملودی از شعر", fontWeight = FontWeight.Bold) }
        }
    }
}

/** External MCP servers: connected by default to Hugging Face (datasets / models / spaces). Add any streamable-HTTP MCP. */
@Composable
private fun McpCard(vm: MainViewModel) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text("اتصال MCP (به‌صورت پیش‌فرض فعال)", color = Amber, fontWeight = FontWeight.SemiBold)
        vm.mcpServers.forEachIndexed { i, srv ->
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Switch(checked = srv.enabled, onCheckedChange = { on -> vm.setMcp(vm.mcpServers.toMutableList().also { it[i] = srv.copy(enabled = on) }) })
                Column(Modifier.weight(1f)) { Text(srv.name, color = Color(0xFFE9E6DF), fontSize = 13.sp); Text(srv.url, color = Color(0xFF6B6D75), fontSize = 10.sp) }
                OutlinedButton(onClick = { vm.setMcp(vm.mcpServers.filterIndexed { j, _ -> j != i }) }) { Text("حذف") }
            }
        }
        var name by remember { mutableStateOf("") }; var url by remember { mutableStateOf("") }; var token by remember { mutableStateOf("") }
        OutlinedTextField(value = name, onValueChange = { name = it }, singleLine = true, label = { Text("نام سرور") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = url, onValueChange = { url = it }, singleLine = true, label = { Text("MCP URL (https://…/mcp)") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = token, onValueChange = { token = it }, singleLine = true, label = { Text("توکن (اختیاری)") }, visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = { if (url.startsWith("http")) { vm.setMcp(vm.mcpServers + McpServer(name.ifBlank { "MCP" }, url.trim(), token.trim())); name = ""; url = ""; token = "" } }) { Text("افزودن") }
            OutlinedButton(onClick = { vm.testMcp() }) { Text("تست اتصال") }
        }
        if (vm.mcpStatus.isNotEmpty()) Text(vm.mcpStatus, color = Color(0xFF9A9CA4), fontSize = 12.sp)
    }
}
