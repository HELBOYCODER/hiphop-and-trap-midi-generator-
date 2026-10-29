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
    fun toggleRecord() {
        if (vm.recording) vm.stopRecording()
        else if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) vm.startRecording()
        else micPermission.launch(Manifest.permission.RECORD_AUDIO)
    }

    Column(Modifier.fillMaxSize().background(Ink).statusBarsPadding().verticalScroll(rememberScrollState()).padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)) {

        Text("HipHop MIDI Studio", color = Amber, fontSize = 26.sp, fontWeight = FontWeight.Bold)
        Text("صدا را بخوان، MIDI بگیر، یا ایده‌ی مشابه بساز", color = Color(0xFF9A9CA4), fontSize = 13.sp)

        // ---- record
        Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
            Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                Button(onClick = { toggleRecord() }, enabled = !vm.busy, modifier = Modifier.fillMaxWidth().height(56.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = if (vm.recording) Color(0xFFE0526B) else Amber, contentColor = Ink)) {
                    Text(if (vm.recording) "■  توقف و تحلیل" else "●  ضبط صدا", fontSize = 18.sp, fontWeight = FontWeight.Bold)
                }
                if (vm.recording) {
                    LinearProgressIndicator(progress = { vm.level.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth(), color = Teal)
                    Text("%.1f ثانیه".format(vm.recordSeconds), color = Color(0xFF9A9CA4), fontSize = 12.sp)
                }
                Text(vm.status, color = Color(0xFFE9E6DF), fontSize = 14.sp)
            }
        }

        val clip = vm.clip
        val tr = vm.transcription
        if (clip != null && tr != null) {
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

            // ---- piano roll + transport
            Card(colors = CardDefaults.cardColors(containerColor = Panel), shape = RoundedCornerShape(12.dp)) {
                Column(Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("پیانو رول  ·  خط فیروزه‌ای = فرکانس صدای شما", color = Amber, fontWeight = FontWeight.SemiBold, fontSize = 13.sp)
                    PianoRoll(clip, tr.contour, tr.startOffsetSec, vm.playTicks, vm.playing)
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
                Text("هوش مصنوعی (Gemini، نیاز به اینترنت)", color = Amber, fontWeight = FontWeight.SemiBold)
                OutlinedTextField(value = vm.apiKey, onValueChange = { vm.apiKey = it }, singleLine = true, label = { Text("Gemini API key") },
                    visualTransformation = PasswordVisualTransformation(), modifier = Modifier.fillMaxWidth())
                OutlinedTextField(value = vm.model, onValueChange = { vm.model = it }, singleLine = true, label = { Text("مدل") }, modifier = Modifier.fillMaxWidth())
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
