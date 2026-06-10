#!/usr/bin/env python3
"""est_serial_helper.py — 串口脚本执行助手

通过 stdin/stdout JSON 协议与 Qt 宿主通信。
用户脚本中可用的函数：
  send(data)       — 发送字符串数据
  send_hex(hex_s)  — 发送 HEX 字符串 (如 "A5 5A 03")
  wait(pattern, timeout=5000) — 等待串口数据匹配模式
  print(msg)       — 输出到终端
  delay(ms)        — 延时（毫秒）
  exit()           — 结束脚本

使用方式：
  由 Qt 应用 (SerialConsolePage) 通过 QProcess 启动，
  通过 stdin 发送 JSON 命令，从 stdout 读取 JSON 事件。
"""

import sys
import json
import time
import threading
import re as _re

# ---------- 内部状态 ----------
_received_buffer = ""
_buffer_lock = threading.Lock()
_emit_lock = threading.Lock()
_running = True
_script_thread = None


class ScriptAbort(Exception):
    pass

# ---------- stdout JSON 输出 ----------
def _emit(obj):
    """发送 JSON 事件到 stdout"""
    line = json.dumps(obj, ensure_ascii=True)
    with _emit_lock:
        sys.stdout.write(line + "\n")
        sys.stdout.flush()


# ---------- 用户脚本可调用的 API ----------

def send(data):
    """发送字符串数据"""
    _emit({"type": "send", "data": data})


def send_hex(hex_str):
    """发送 HEX 字符串（空格分隔）"""
    cleaned = hex_str.replace(" ", "").replace("0x", "").replace("0X", "")
    _emit({"type": "send_hex", "data": cleaned})


def wait(pattern, timeout=5000):
    """等待串口数据匹配指定模式（正则表达式）

    参数:
        pattern: 正则表达式或普通字符串
        timeout: 超时毫秒（默认 5000ms）

    返回:
        匹配到的文本，超时返回空字符串
    """
    global _received_buffer

    _emit({"type": "wait_start", "pattern": pattern, "timeout": timeout})

    try:
        regex = _re.compile(pattern.encode("utf-8", errors="replace"))
    except _re.error:
        regex = _re.compile(_re.escape(pattern.encode("utf-8", errors="replace")))

    deadline = time.time() + timeout / 1000.0
    matched = ""

    while _running and time.time() < deadline:
        with _buffer_lock:
            if _received_buffer:
                m = regex.search(_received_buffer.encode("utf-8", errors="replace"))
                if m:
                    matched = m.group(0).decode("utf-8", errors="replace")
                    # 消耗匹配到的数据
                    pos = m.end()
                    remaining = _received_buffer[pos:]
                    _received_buffer = remaining

        if matched:
            break

        time.sleep(0.01)  # 10ms polling

    result = matched if matched else ""
    _emit({"type": "wait_result", "found": bool(matched), "text": result})
    return result


def print(msg):
    """输出消息到终端"""
    _emit({"type": "print", "msg": str(msg)})


def delay(ms):
    """延时指定毫秒"""
    _emit({"type": "delay", "ms": ms})
    deadline = time.time() + ms / 1000.0
    while _running and time.time() < deadline:
        time.sleep(min(0.05, deadline - time.time()))


def exit():
    """结束脚本执行"""
    global _running
    _running = False
    _emit({"type": "finished", "success": True})
    sys.exit(0)


# ---------- 辅助函数 ----------

def _feed_data(payload):
    """将串口接收到的数据追加到缓冲区"""
    global _received_buffer
    with _buffer_lock:
        # 限制缓冲区大小
        _received_buffer += payload
        if len(_received_buffer) > 65536:
            _received_buffer = _received_buffer[-32768:]


def _trace_abort(frame, event, arg):
    if not _running:
        raise ScriptAbort()
    return _trace_abort


def _run_script(script):
    global _running

    sys.settrace(_trace_abort)
    try:
        globals_dict = {
            "send": send,
            "send_hex": send_hex,
            "wait": wait,
            "print": print,
            "delay": delay,
            "exit": exit,
            "re": _re,
            "__builtins__": {
                "True": True, "False": False, "None": None,
                "int": int, "float": float, "str": str,
                "bytes": bytes, "bytearray": bytearray,
                "list": list, "dict": dict, "tuple": tuple,
                "len": len, "range": range, "min": min, "max": max,
                "abs": abs, "ord": ord, "chr": chr,
                "hex": hex, "bin": bin, "oct": oct,
                "isinstance": isinstance, "type": type,
                "enumerate": enumerate, "zip": zip, "map": map,
                "Exception": Exception, "BaseException": BaseException,
                "KeyboardInterrupt": KeyboardInterrupt,
            }
        }
        exec(script, globals_dict)
        if _running:
            _emit({"type": "finished", "success": True})
    except ScriptAbort:
        _emit({"type": "finished", "success": False, "msg": "脚本被用户终止"})
    except SystemExit:
        _emit({"type": "finished", "success": True})
    except Exception as e:
        _emit({"type": "error", "msg": f"{type(e).__name__}: {e}"})
        _emit({"type": "finished", "success": False})
    finally:
        sys.settrace(None)
        _running = False


# ---------- 主循环 ----------

def _main():
    global _running, _script_thread

    _emit({"type": "ready"})

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            msg = json.loads(line)
        except json.JSONDecodeError:
            _emit({"type": "error", "msg": f"JSON 解析错误: {line[:100]}"})
            continue

        cmd = msg.get("cmd", "")

        if cmd == "run":
            # 执行用户脚本
            script = msg.get("script", "")
            if _script_thread and _script_thread.is_alive():
                _emit({"type": "error", "msg": "已有脚本正在运行"})
                continue

            _running = True
            _script_thread = threading.Thread(target=_run_script, args=(script,), daemon=True)
            _script_thread.start()

        elif cmd == "abort":
            _running = False
            _emit({"type": "print", "msg": "正在停止脚本..."})

        elif cmd == "data":
            payload = msg.get("payload", "")
            _feed_data(payload)

        elif cmd == "shutdown":
            _running = False
            break

    _running = False


if __name__ == "__main__":
    try:
        _main()
    except KeyboardInterrupt:
        pass
    except BrokenPipeError:
        pass
