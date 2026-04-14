import socket
import threading
import customtkinter as ctk
from tkinter import messagebox, simpledialog

ctk.set_appearance_mode("light")
ctk.set_default_color_theme("blue")

class ChatClientUI:
    def __init__(self):
        self.root = ctk.CTk()
        self.root.title("网络聊天室 - 客户端")
        self.root.geometry("900x700")
        self.root.resizable(True, True)
        
        self.client_socket = None
        self.running = False
        self.username = ""
        self.current_room = "公共聊天室"
        
        self.setup_login_ui()
        
    def setup_login_ui(self):
        self.login_frame = ctk.CTkFrame(self.root, corner_radius=15)
        self.login_frame.pack(expand=True, fill="both", padx=50, pady=50)
        
        title_label = ctk.CTkLabel(
            self.login_frame, 
            text="网络聊天室", 
            font=ctk.CTkFont(size=32, weight="bold")
        )
        title_label.pack(pady=(40, 10))
        
        subtitle_label = ctk.CTkLabel(
            self.login_frame, 
            text="请输入连接信息", 
            font=ctk.CTkFont(size=14)
        )
        subtitle_label.pack(pady=(0, 40))
        
        input_frame = ctk.CTkFrame(self.login_frame, fg_color="transparent")
        input_frame.pack(expand=True, fill="x", padx=50)
        
        self.username_entry = ctk.CTkEntry(
            input_frame, 
            placeholder_text="请输入昵称",
            height=45,
            font=ctk.CTkFont(size=14)
        )
        self.username_entry.pack(fill="x", pady=10)
        
        self.ip_entry = ctk.CTkEntry(
            input_frame, 
            placeholder_text="服务器IP地址 (如: 127.0.0.1)",
            height=45,
            font=ctk.CTkFont(size=14)
        )
        self.ip_entry.pack(fill="x", pady=10)
        self.ip_entry.insert(0, "127.0.0.1")
        
        self.port_entry = ctk.CTkEntry(
            input_frame, 
            placeholder_text="端口号 (如: 8888)",
            height=45,
            font=ctk.CTkFont(size=14)
        )
        self.port_entry.pack(fill="x", pady=10)
        self.port_entry.insert(0, "8888")
        
        self.connect_btn = ctk.CTkButton(
            input_frame,
            text="连接服务器",
            height=50,
            font=ctk.CTkFont(size=16, weight="bold"),
            command=self.connect_to_server
        )
        self.connect_btn.pack(fill="x", pady=(30, 10))
        
        self.status_label = ctk.CTkLabel(
            input_frame,
            text="",
            font=ctk.CTkFont(size=12),
            text_color="gray"
        )
        self.status_label.pack(pady=5)
        
    def connect_to_server(self):
        username = self.username_entry.get().strip()
        ip = self.ip_entry.get().strip()
        port_str = self.port_entry.get().strip()
        
        if not username:
            messagebox.showerror("错误", "请输入昵称！")
            return
        if not ip:
            messagebox.showerror("错误", "请输入服务器IP地址！")
            return
        if not port_str:
            messagebox.showerror("错误", "请输入端口号！")
            return
            
        try:
            port = int(port_str)
        except ValueError:
            messagebox.showerror("错误", "端口号必须是数字！")
            return
            
        self.username = username
        self.status_label.configure(text="正在连接...", text_color="blue")
        self.connect_btn.configure(state="disabled")
        
        try:
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.connect((ip, port))
            self.client_socket.send(username.encode('gbk'))
            
            self.running = True
            self.receive_thread = threading.Thread(target=self.receive_messages, daemon=True)
            self.receive_thread.start()
            
            self.status_label.configure(text="连接成功！", text_color="green")
            self.root.after(500, self.switch_to_chat_ui)
            
        except Exception as e:
            self.status_label.configure(text=f"连接失败: {str(e)}", text_color="red")
            self.connect_btn.configure(state="normal")
            messagebox.showerror("连接失败", f"无法连接到服务器: {str(e)}")
            
    def switch_to_chat_ui(self):
        self.login_frame.pack_forget()
        self.setup_chat_ui()
        
    def setup_chat_ui(self):
        self.main_frame = ctk.CTkFrame(self.root, corner_radius=0)
        self.main_frame.pack(expand=True, fill="both")
        
        self.main_frame.grid_rowconfigure(0, weight=1)
        self.main_frame.grid_columnconfigure(0, weight=3)
        self.main_frame.grid_columnconfigure(1, weight=1)
        
        left_frame = ctk.CTkFrame(self.main_frame, corner_radius=0)
        left_frame.grid(row=0, column=0, sticky="nsew", padx=(10, 5), pady=10)
        
        right_frame = ctk.CTkFrame(self.main_frame, corner_radius=0)
        right_frame.grid(row=0, column=1, sticky="nsew", padx=(5, 10), pady=10)
        
        left_frame.grid_rowconfigure(1, weight=1)
        left_frame.grid_columnconfigure(0, weight=1)
        
        header_frame = ctk.CTkFrame(left_frame, corner_radius=10)
        header_frame.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        
        self.room_label = ctk.CTkLabel(
            header_frame,
            text=f"📍 {self.current_room}",
            font=ctk.CTkFont(size=18, weight="bold")
        )
        self.room_label.pack(side="left", padx=15, pady=10)
        
        self.user_label = ctk.CTkLabel(
            header_frame,
            text=f"👤 {self.username}",
            font=ctk.CTkFont(size=14)
        )
        self.user_label.pack(side="right", padx=15, pady=10)
        
        chat_frame = ctk.CTkFrame(left_frame, corner_radius=10)
        chat_frame.grid(row=1, column=0, sticky="nsew")
        
        chat_frame.grid_rowconfigure(0, weight=1)
        chat_frame.grid_columnconfigure(0, weight=1)
        
        self.chat_text = ctk.CTkTextbox(
            chat_frame,
            font=ctk.CTkFont(size=13),
            corner_radius=10,
            state="disabled"
        )
        self.chat_text.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        
        input_frame = ctk.CTkFrame(left_frame, corner_radius=10)
        input_frame.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        
        self.message_entry = ctk.CTkEntry(
            input_frame,
            placeholder_text="输入消息...",
            height=45,
            font=ctk.CTkFont(size=14)
        )
        self.message_entry.pack(side="left", fill="x", expand=True, padx=(10, 10), pady=10)
        self.message_entry.bind("<Return>", lambda event: self.send_message())
        
        self.send_btn = ctk.CTkButton(
            input_frame,
            text="发送",
            width=100,
            height=45,
            font=ctk.CTkFont(size=14, weight="bold"),
            command=self.send_message
        )
        self.send_btn.pack(side="right", padx=(0, 10), pady=10)
        
        right_frame.grid_rowconfigure(2, weight=1)
        right_frame.grid_columnconfigure(0, weight=1)
        
        menu_label = ctk.CTkLabel(
            right_frame,
            text="功能菜单",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        menu_label.grid(row=0, column=0, pady=(10, 5))
        
        menu_frame = ctk.CTkFrame(right_frame, corner_radius=10)
        menu_frame.grid(row=1, column=0, sticky="ew", padx=10, pady=(0, 10))
        
        btn_style = {
            "height": 40,
            "font": ctk.CTkFont(size=13),
            "corner_radius": 8
        }
        
        self.create_room_btn = ctk.CTkButton(
            menu_frame,
            text="🏠 创建私有聊天室",
            command=self.create_private_room,
            **btn_style
        )
        self.create_room_btn.pack(fill="x", padx=10, pady=8)
        
        self.join_room_btn = ctk.CTkButton(
            menu_frame,
            text="🚪 加入私有聊天室",
            command=self.join_private_room,
            **btn_style
        )
        self.join_room_btn.pack(fill="x", padx=10, pady=8)
        
        self.leave_room_btn = ctk.CTkButton(
            menu_frame,
            text="↩️ 离开私有聊天室",
            command=self.leave_private_room,
            **btn_style
        )
        self.leave_room_btn.pack(fill="x", padx=10, pady=8)
        
        self.list_rooms_btn = ctk.CTkButton(
            menu_frame,
            text="📋 查看所有聊天室",
            command=self.list_rooms,
            **btn_style
        )
        self.list_rooms_btn.pack(fill="x", padx=10, pady=8)
        
        self.help_btn = ctk.CTkButton(
            menu_frame,
            text="❓ 帮助信息",
            command=self.show_help,
            **btn_style
        )
        self.help_btn.pack(fill="x", padx=10, pady=8)
        
        self.disconnect_btn = ctk.CTkButton(
            menu_frame,
            text="🚪 退出聊天室",
            command=self.disconnect,
            fg_color="#dc3545",
            hover_color="#c82333",
            **btn_style
        )
        self.disconnect_btn.pack(fill="x", padx=10, pady=8)
        
        info_label = ctk.CTkLabel(
            right_frame,
            text="系统消息",
            font=ctk.CTkFont(size=16, weight="bold")
        )
        info_label.grid(row=2, column=0, pady=(10, 5), sticky="s")
        
        info_frame = ctk.CTkFrame(right_frame, corner_radius=10)
        info_frame.grid(row=3, column=0, sticky="ew", padx=10, pady=(0, 10))
        
        self.info_text = ctk.CTkTextbox(
            info_frame,
            height=150,
            font=ctk.CTkFont(size=12),
            corner_radius=10,
            state="disabled"
        )
        self.info_text.pack(fill="both", expand=True, padx=10, pady=10)
        
    def receive_messages(self):
        while self.running:
            try:
                message = self.client_socket.recv(1024).decode('gbk')
                if message:
                    self.root.after(0, lambda m=message: self.display_message(m))
            except:
                if self.running:
                    self.root.after(0, self.handle_disconnect)
                break
                
    def display_message(self, message):
        if "[系统]" in message:
            if "房间号：" in message:
                room_id = message.split("房间号：")[-1].strip()
                self.current_room = f"私有聊天室 {room_id}"
                self.room_label.configure(text=f"📍 {self.current_room}")
            elif "成功加入房间" in message:
                room_id = message.split("房间 ")[-1].strip()
                self.current_room = f"私有聊天室 {room_id}"
                self.room_label.configure(text=f"📍 {self.current_room}")
            elif "回到公共聊天室" in message:
                self.current_room = "公共聊天室"
                self.room_label.configure(text=f"📍 {self.current_room}")
            
            self.info_text.configure(state="normal")
            self.info_text.insert("end", message + "\n\n")
            self.info_text.see("end")
            self.info_text.configure(state="disabled")
        else:
            self.chat_text.configure(state="normal")
            self.chat_text.insert("end", message + "\n\n")
            self.chat_text.see("end")
            self.chat_text.configure(state="disabled")
            
    def send_message(self):
        message = self.message_entry.get().strip()
        if not message:
            return
            
        try:
            self.client_socket.send(message.encode('gbk'))
            self.message_entry.delete(0, "end")
            
            if message == "q":
                self.disconnect()
        except:
            messagebox.showerror("错误", "发送消息失败！")
            
    def create_private_room(self):
        try:
            self.client_socket.send("/create".encode('gbk'))
        except:
            messagebox.showerror("错误", "发送命令失败！")
            
    def join_private_room(self):
        room_id = simpledialog.askstring("加入房间", "请输入4位房间号:")
        if room_id:
            if len(room_id) != 4 or not room_id.isdigit():
                messagebox.showerror("错误", "房间号必须是4位数字！")
                return
            try:
                self.client_socket.send(f"/join {room_id}".encode('gbk'))
            except:
                messagebox.showerror("错误", "发送命令失败！")
                
    def leave_private_room(self):
        try:
            self.client_socket.send("/leave".encode('gbk'))
        except:
            messagebox.showerror("错误", "发送命令失败！")
            
    def list_rooms(self):
        try:
            self.client_socket.send("/list".encode('gbk'))
        except:
            messagebox.showerror("错误", "发送命令失败！")
            
    def show_help(self):
        try:
            self.client_socket.send("/help".encode('gbk'))
        except:
            messagebox.showerror("错误", "发送命令失败！")
            
    def handle_disconnect(self):
        self.running = False
        messagebox.showinfo("提示", "与服务器的连接已断开！")
        self.root.quit()
        
    def disconnect(self):
        self.running = False
        try:
            self.client_socket.send("q".encode('gbk'))
            self.client_socket.close()
        except:
            pass
        self.root.quit()
        
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    app = ChatClientUI()
    app.run()
