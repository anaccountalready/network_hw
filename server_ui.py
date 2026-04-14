import socket
import threading
import customtkinter as ctk
from tkinter import messagebox
import time
import random
import string

ctk.set_appearance_mode("light")
ctk.set_default_color_theme("blue")

class ChatServerUI:
    def __init__(self):
        self.root = ctk.CTk()
        self.root.title("网络聊天室 - 服务器")
        self.root.geometry("900x700")
        self.root.resizable(True, True)
        
        self.server_socket = None
        self.running = False
        self.clients = {}
        self.client_names = {}
        self.private_rooms = {}
        self.client_rooms = {}
        
        self.setup_ui()
        
    def setup_ui(self):
        self.main_frame = ctk.CTkFrame(self.root, corner_radius=0)
        self.main_frame.pack(expand=True, fill="both", padx=10, pady=10)
        
        self.main_frame.grid_rowconfigure(1, weight=1)
        self.main_frame.grid_columnconfigure(0, weight=3)
        self.main_frame.grid_columnconfigure(1, weight=1)
        
        control_frame = ctk.CTkFrame(self.main_frame, corner_radius=10)
        control_frame.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 10))
        
        title_label = ctk.CTkLabel(
            control_frame,
            text="网络聊天室服务器",
            font=ctk.CTkFont(size=24, weight="bold")
        )
        title_label.pack(side="left", padx=20, pady=15)
        
        port_frame = ctk.CTkFrame(control_frame, fg_color="transparent")
        port_frame.pack(side="left", padx=20)
        
        port_label = ctk.CTkLabel(
            port_frame,
            text="端口号:",
            font=ctk.CTkFont(size=14)
        )
        port_label.pack(side="left", padx=(0, 10))
        
        self.port_entry = ctk.CTkEntry(
            port_frame,
            width=100,
            height=35,
            font=ctk.CTkFont(size=14)
        )
        self.port_entry.pack(side="left")
        self.port_entry.insert(0, "8888")
        
        self.start_btn = ctk.CTkButton(
            control_frame,
            text="🚀 启动服务器",
            width=150,
            height=40,
            font=ctk.CTkFont(size=14, weight="bold"),
            command=self.start_server
        )
        self.start_btn.pack(side="right", padx=20, pady=10)
        
        self.stop_btn = ctk.CTkButton(
            control_frame,
            text="⏹️ 停止服务器",
            width=150,
            height=40,
            font=ctk.CTkFont(size=14, weight="bold"),
            command=self.stop_server,
            state="disabled",
            fg_color="#dc3545",
            hover_color="#c82333"
        )
        self.stop_btn.pack(side="right", padx=(0, 10), pady=10)
        
        left_frame = ctk.CTkFrame(self.main_frame, corner_radius=10)
        left_frame.grid(row=1, column=0, sticky="nsew", padx=(0, 5))
        
        left_frame.grid_rowconfigure(1, weight=1)
        left_frame.grid_columnconfigure(0, weight=1)
        
        log_label = ctk.CTkLabel(
            left_frame,
            text="服务器日志",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        log_label.grid(row=0, column=0, pady=(10, 5))
        
        log_frame = ctk.CTkFrame(left_frame, corner_radius=10)
        log_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=(0, 10))
        
        self.log_text = ctk.CTkTextbox(
            log_frame,
            font=ctk.CTkFont(size=12),
            corner_radius=10,
            state="disabled"
        )
        self.log_text.pack(fill="both", expand=True, padx=10, pady=10)
        
        right_frame = ctk.CTkFrame(self.main_frame, corner_radius=10)
        right_frame.grid(row=1, column=1, sticky="nsew", padx=(5, 0))
        
        right_frame.grid_rowconfigure(1, weight=1)
        right_frame.grid_rowconfigure(3, weight=1)
        right_frame.grid_columnconfigure(0, weight=1)
        
        users_label = ctk.CTkLabel(
            right_frame,
            text="在线用户",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        users_label.grid(row=0, column=0, pady=(10, 5))
        
        users_frame = ctk.CTkFrame(right_frame, corner_radius=10)
        users_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=(0, 10))
        
        self.users_list = ctk.CTkTextbox(
            users_frame,
            font=ctk.CTkFont(size=12),
            corner_radius=10,
            state="disabled"
        )
        self.users_list.pack(fill="both", expand=True, padx=10, pady=10)
        
        rooms_label = ctk.CTkLabel(
            right_frame,
            text="私有聊天室",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        rooms_label.grid(row=2, column=0, pady=(10, 5))
        
        rooms_frame = ctk.CTkFrame(right_frame, corner_radius=10)
        rooms_frame.grid(row=3, column=0, sticky="nsew", padx=10, pady=(0, 10))
        
        self.rooms_list = ctk.CTkTextbox(
            rooms_frame,
            font=ctk.CTkFont(size=12),
            corner_radius=10,
            state="disabled"
        )
        self.rooms_list.pack(fill="both", expand=True, padx=10, pady=10)
        
        status_frame = ctk.CTkFrame(self.main_frame, corner_radius=10)
        status_frame.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(10, 0))
        
        self.status_label = ctk.CTkLabel(
            status_frame,
            text="服务器状态: 未启动",
            font=ctk.CTkFont(size=14),
            text_color="gray"
        )
        self.status_label.pack(side="left", padx=20, pady=10)
        
        self.online_count_label = ctk.CTkLabel(
            status_frame,
            text="在线用户: 0",
            font=ctk.CTkFont(size=14)
        )
        self.online_count_label.pack(side="right", padx=20, pady=10)
        
    def log_message(self, message):
        timestamp = time.strftime("%H:%M:%S")
        full_message = f"[{timestamp}] {message}"
        self.log_text.configure(state="normal")
        self.log_text.insert("end", full_message + "\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")
        
    def update_users_list(self):
        self.users_list.configure(state="normal")
        self.users_list.delete("1.0", "end")
        
        if not self.client_names:
            self.users_list.insert("end", "暂无在线用户\n")
        else:
            for sock, name in self.client_names.items():
                room = self.client_rooms.get(sock, "public")
                room_display = "公共聊天室" if room == "public" else f"房间 {room}"
                self.users_list.insert("end", f"👤 {name} ({room_display})\n")
                
        self.users_list.configure(state="disabled")
        self.online_count_label.configure(text=f"在线用户: {len(self.client_names)}")
        
    def update_rooms_list(self):
        self.rooms_list.configure(state="normal")
        self.rooms_list.delete("1.0", "end")
        
        if not self.private_rooms:
            self.rooms_list.insert("end", "暂无私有聊天室\n")
        else:
            for room_id, members in self.private_rooms.items():
                member_names = [self.client_names.get(sock, str(sock)) for sock in members]
                self.rooms_list.insert("end", f"🏠 房间 {room_id}\n")
                self.rooms_list.insert("end", f"   人数: {len(members)}\n")
                self.rooms_list.insert("end", f"   成员: {', '.join(member_names)}\n\n")
                
        self.rooms_list.configure(state="disabled")
        
    def generate_room_id(self):
        while True:
            room_id = ''.join(random.choices(string.digits, k=4))
            if room_id not in self.private_rooms:
                return room_id
                
    def send_to_client(self, sock, message):
        try:
            sock.send(message.encode('gbk'))
        except:
            pass
            
    def broadcast_to_all(self, message, exclude_sock=None):
        for sock in self.clients:
            if sock != exclude_sock:
                self.send_to_client(sock, message)
                
    def broadcast_to_room(self, room_id, message, exclude_sock=None):
        if room_id == "public":
            for sock in self.clients:
                if sock != exclude_sock:
                    self.send_to_client(sock, message)
        else:
            if room_id in self.private_rooms:
                for sock in self.private_rooms[room_id]:
                    if sock != exclude_sock:
                        self.send_to_client(sock, message)
                        
    def handle_command(self, sender_sock, command):
        parts = command.split()
        cmd = parts[0]
        
        sender_name = self.client_names.get(sender_sock, "未知用户")
        current_room = self.client_rooms.get(sender_sock, "public")
        
        if cmd == "/create":
            room_id = self.generate_room_id()
            self.private_rooms[room_id] = {sender_sock}
            
            if current_room != "public":
                self.private_rooms[current_room].discard(sender_sock)
                self.broadcast_to_room(current_room, f"[系统] {sender_name} 离开了房间")
                if not self.private_rooms[current_room]:
                    del self.private_rooms[current_room]
                    self.log_message(f"私有聊天室 {current_room} 已解散")
            else:
                self.broadcast_to_all(f"[系统] {sender_name} 离开了公共聊天室", sender_sock)
                
            self.client_rooms[sender_sock] = room_id
            
            response = f"[系统] 私有聊天室创建成功！房间号：{room_id}"
            self.send_to_client(sender_sock, response)
            self.log_message(f"用户 {sender_name} 创建了私有聊天室 {room_id}")
            self.update_rooms_list()
            self.update_users_list()
            
        elif cmd == "/join":
            if len(parts) < 2:
                self.send_to_client(sender_sock, "[系统] 请输入房间号！")
                return
                
            room_id = parts[1]
            
            if len(room_id) != 4 or not room_id.isdigit():
                self.send_to_client(sender_sock, "[系统] 房间号必须是4位数字！")
                return
                
            if room_id not in self.private_rooms:
                self.send_to_client(sender_sock, "[系统] 房间不存在！")
                return
                
            if current_room == room_id:
                self.send_to_client(sender_sock, "[系统] 您已经在这个房间了！")
                return
                
            if current_room != "public":
                self.private_rooms[current_room].discard(sender_sock)
                self.broadcast_to_room(current_room, f"[系统] {sender_name} 离开了房间")
                if not self.private_rooms[current_room]:
                    del self.private_rooms[current_room]
                    self.log_message(f"私有聊天室 {current_room} 已解散")
            else:
                self.broadcast_to_all(f"[系统] {sender_name} 离开了公共聊天室", sender_sock)
                
            self.client_rooms[sender_sock] = room_id
            self.private_rooms[room_id].add(sender_sock)
            
            self.broadcast_to_room(room_id, f"[系统] {sender_name} 加入了房间")
            self.send_to_client(sender_sock, f"[系统] 成功加入房间 {room_id}")
            self.log_message(f"用户 {sender_name} 加入了私有聊天室 {room_id}")
            self.update_rooms_list()
            self.update_users_list()
            
        elif cmd == "/leave":
            if current_room == "public":
                self.send_to_client(sender_sock, "[系统] 您当前在公共聊天室，无法离开！")
                return
                
            self.private_rooms[current_room].discard(sender_sock)
            self.broadcast_to_room(current_room, f"[系统] {sender_name} 离开了房间")
            
            if not self.private_rooms[current_room]:
                del self.private_rooms[current_room]
                self.log_message(f"私有聊天室 {current_room} 已解散")
                
            self.client_rooms[sender_sock] = "public"
            self.broadcast_to_all(f"[系统] {sender_name} 回到了公共聊天室")
            self.send_to_client(sender_sock, "[系统] 您已回到公共聊天室")
            self.log_message(f"用户 {sender_name} 回到了公共聊天室")
            self.update_rooms_list()
            self.update_users_list()
            
        elif cmd == "/list":
            response = "[系统] 当前私有聊天室列表：\n"
            if not self.private_rooms:
                response += "  暂无私有聊天室\n"
            else:
                for room_id, members in self.private_rooms.items():
                    response += f"  房间号: {room_id} (人数: {len(members)})\n"
            self.send_to_client(sender_sock, response)
            
        elif cmd == "/help":
            help_text = "[系统] 可用命令：\n"
            help_text += "  /create - 创建私有聊天室\n"
            help_text += "  /join <房间号> - 加入私有聊天室\n"
            help_text += "  /leave - 离开私有聊天室，回到公共聊天室\n"
            help_text += "  /list - 查看所有私有聊天室\n"
            help_text += "  /help - 显示帮助信息\n"
            self.send_to_client(sender_sock, help_text)
            
        elif cmd == "/menu":
            menu_text = "\n========== 聊天室菜单 ==========\n"
            menu_text += f"当前位置: {'公共聊天室' if current_room == 'public' else '私有聊天室 ' + current_room}\n"
            menu_text += "1. 创建私有聊天室 (/create)\n"
            menu_text += "2. 加入私有聊天室 (/join <房间号>)\n"
            menu_text += "3. 离开私有聊天室 (/leave)\n"
            menu_text += "4. 查看所有私有聊天室 (/list)\n"
            menu_text += "5. 显示帮助信息 (/help)\n"
            menu_text += "6. 退出聊天室 (q)\n"
            menu_text += "================================\n"
            self.send_to_client(sender_sock, menu_text)
            
        else:
            self.send_to_client(sender_sock, "[系统] 未知命令，输入 /help 查看帮助")
            
    def handle_client(self, client_sock, client_addr):
        client_ip, client_port = client_addr
        self.log_message(f"新连接来自: {client_ip}:{client_port}")
        
        try:
            name_data = client_sock.recv(1024)
            if not name_data:
                client_sock.close()
                return
                
            username = name_data.decode('gbk')
            self.client_names[client_sock] = username
            self.client_rooms[client_sock] = "public"
            
            welcome = f"[系统] 欢迎 {username} 加入公共聊天室！\n"
            welcome += "[系统] 输入 /menu 查看菜单，/help 查看帮助\n"
            self.send_to_client(client_sock, welcome)
            
            self.broadcast_to_all(f"[系统] {username} 加入了公共聊天室", client_sock)
            self.log_message(f"用户 {username} 已登录")
            self.update_users_list()
            
            while self.running:
                try:
                    data = client_sock.recv(1024)
                    if not data:
                        break
                        
                    message = data.decode('gbk')
                    self.log_message(f"收到来自 {username} 的消息: {message}")
                    
                    if message.startswith("/"):
                        self.handle_command(client_sock, message)
                    elif message == "q":
                        break
                    else:
                        current_room = self.client_rooms.get(client_sock, "public")
                        if current_room == "public":
                            room_display = "公共"
                            full_message = f"[公共] {username} 说: {message}"
                            self.broadcast_to_all(full_message, client_sock)
                        else:
                            room_display = current_room
                            full_message = f"[{room_display}] {username} 说: {message}"
                            self.broadcast_to_room(current_room, full_message, client_sock)
                        
                except Exception as e:
                    self.log_message(f"接收消息时出错: {str(e)}")
                    break
                    
        except Exception as e:
            self.log_message(f"处理客户端时出错: {str(e)}")
            
        finally:
            username = self.client_names.get(client_sock, "未知用户")
            current_room = self.client_rooms.get(client_sock, "public")
            
            if current_room != "public" and current_room in self.private_rooms:
                self.private_rooms[current_room].discard(client_sock)
                self.broadcast_to_room(current_room, f"[系统] {username} 离开了房间")
                if not self.private_rooms[current_room]:
                    del self.private_rooms[current_room]
                    self.log_message(f"私有聊天室 {current_room} 已解散")
            else:
                self.broadcast_to_all(f"{username} 离开了聊天室", client_sock)
                
            if client_sock in self.clients:
                del self.clients[client_sock]
            if client_sock in self.client_names:
                del self.client_names[client_sock]
            if client_sock in self.client_rooms:
                del self.client_rooms[client_sock]
                
            try:
                client_sock.close()
            except:
                pass
                
            self.log_message(f"用户 {username} 已断开连接")
            self.update_users_list()
            self.update_rooms_list()
            
    def accept_clients(self):
        while self.running:
            try:
                client_sock, client_addr = self.server_socket.accept()
                self.clients[client_sock] = client_addr
                
                client_thread = threading.Thread(
                    target=self.handle_client,
                    args=(client_sock, client_addr),
                    daemon=True
                )
                client_thread.start()
                
            except Exception as e:
                if self.running:
                    self.log_message(f"接受连接时出错: {str(e)}")
                break
                
    def start_server(self):
        port_str = self.port_entry.get().strip()
        
        if not port_str:
            messagebox.showerror("错误", "请输入端口号！")
            return
            
        try:
            port = int(port_str)
        except ValueError:
            messagebox.showerror("错误", "端口号必须是数字！")
            return
            
        try:
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.server_socket.bind(("0.0.0.0", port))
            self.server_socket.listen(5)
            
            self.running = True
            
            accept_thread = threading.Thread(target=self.accept_clients, daemon=True)
            accept_thread.start()
            
            self.start_btn.configure(state="disabled")
            self.stop_btn.configure(state="normal")
            self.port_entry.configure(state="disabled")
            self.status_label.configure(text=f"服务器状态: 运行中 (端口: {port})", text_color="green")
            
            self.log_message(f"服务器已启动，监听端口: {port}")
            self.log_message("等待客户端连接...")
            
        except Exception as e:
            messagebox.showerror("错误", f"启动服务器失败: {str(e)}")
            
    def stop_server(self):
        self.running = False
        
        for client_sock in list(self.clients.keys()):
            try:
                client_sock.close()
            except:
                pass
                
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
                
        self.clients.clear()
        self.client_names.clear()
        self.client_rooms.clear()
        self.private_rooms.clear()
        
        self.start_btn.configure(state="normal")
        self.stop_btn.configure(state="disabled")
        self.port_entry.configure(state="normal")
        self.status_label.configure(text="服务器状态: 未启动", text_color="gray")
        
        self.log_message("服务器已停止")
        self.update_users_list()
        self.update_rooms_list()
        
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    app = ChatServerUI()
    app.run()
