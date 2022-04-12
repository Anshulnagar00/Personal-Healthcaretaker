import firebase_admin
from firebase_admin import credentials
from firebase_admin import db
from datetime import datetime
import  telebot
import re
import pytz

from telebot import types
from random import random

# https://stackoverflow.com/questions/45558984/how-to-make-telegram-bot-dynamic-keyboardbutton-in-python-every-button-on-one-ro
# Code ref link

ist1 = pytz.timezone('Asia/Calcutta')
# Setting timezone to IST

json_path = r'personal-healthcaretaker-firebase-adminsdk-kjfeh-8e5c7f3a18.json'
# Path to your firebase project JSON configurations file
cred = credentials.Certificate(json_path)
obj = firebase_admin.initialize_app(cred,{
'databaseURL':'https://personal-healthcaretaker-default-rtdb.firebaseio.com/'})
# Make python script connect to firebase realtime database as admin access

TELEGRAM_BOT_API_TOKET = "replace with your telegram bot token"
bot = telebot.TeleBot(TELEGRAM_BOT_API_TOKET)

firstcall = True

buffer = {}

def listener(event):
    global firstcall
    #print(event.event_type)
    #print(event.path)
    #print(event.data)
    contact_no = []
    Message = ""
    if not firstcall:
        try :
            sos = db.reference('Devices/'+event.path[1:].split('/')[0]+'/Data/SOS Details').get()
            print(sos['Contact No'].split(','))
            contact_no = sos['Contact No'].split(',')
            Message  = sos['Sos Message']
        except :
            pass
        #print((contact_no,Message))
        #print(event.path[1:])
        #print(event.path.split('/')[1]+'/Data/SOS call')
        if (event.path[1:] == event.path.split('/')[1]+'/Data/SOS call'):
            print("[ALERT] message api tiggered")
            for i in contact_no:
                if len(i) > 5 :
                    try:
                        bot.send_message(i,Message)
                    except:
                        print("[ALERT] failed to send message")

    firstcall = False


db.reference('Devices', app= obj).listen(listener)


operationsList = {"AT":"Alarm Times","SM":"Messages","GT":"Temperature Readings","GP":"Pulse Oximetry Readings"}

def makeDeviceSelectKeyboard():
    markup = types.InlineKeyboardMarkup()
    if True :
        if True:
            User_id = db.reference('Telegram_IDs/'+str(message.from_user.id), app= obj).get()
            #print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]

                if Verf_Dev:
                    for key, value in Verf_Dev.items():
                        markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data="[[--DEVICE_SELECT "+key+","+value['Name']))

            return markup
    return None

def makeOperationsKeyboard():
    markup = types.InlineKeyboardMarkup()
    for key, value in operationsList.items():
        markup.add(types.InlineKeyboardButton(text=value,
                                              callback_data="[[--OPTION-SELECTED "+key))

    return markup


@bot.callback_query_handler(func=lambda call: True)
def handle_query(call):

    if (call.data.startswith("[[--OPTION-SELECTED ")):
        print(call.data[20:])
        rdata = call.data[20:].split(",")
        try:
            User_id = db.reference('Telegram_IDs/'+str(call.from_user.id), app= obj).get()
            print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]
                callback_request = ""
                if rdata[0] == "AT":
                    callback_request = "[[--GET-ALARM-TIMES "
                elif rdata[0] == "SM":
                    callback_request = "[[--GET-NOTES "
                elif rdata[0] == "GT":
                    callback_request = "[[--GET-TR "
                elif rdata[0] == "GP":
                    callback_request = "[[--GET-POR "

                if Verf_Dev:
                    markup = types.InlineKeyboardMarkup()
                    for key, value in Verf_Dev.items():
                        markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data=callback_request+key+","+value['Name']))
                    bot.send_message(chat_id=call.message.chat.id,
                     text="Select device",
                     reply_markup=markup,
                     parse_mode='HTML')
                else:
                    bot.send_message(chat_id=call.message.chat.id,
                    text="No Devices Found !")

        except Exception as e:
            bot.answer_callback_query(callback_query_id=call.id,
                              show_alert=True,
                              text="[ERROR] "+str(e))

    if (call.data.startswith("[[--GET-ALARM-TIMES ")):
        print(call.data[20:])
        rdata = call.data[20:].split(",")
        try:
            data = db.reference('Devices/'+rdata[0]+'/Data/medicine alarm time', app= obj).get()
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="All medicine alarm times of "+rdata[1]+" device is - "+data,
                              message_id=call.message.message_id,
                              )
        except Exception as e:
            bot.answer_callback_query(callback_query_id=call.id,
                              show_alert=True,
                              text="[ERROR] "+str(e))

    if (call.data.startswith("[[--GET-TR ")):
        print(call.data[11:])
        rdata = call.data[11:].split(",")
        try:
            data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Temperature_logs_CIV', app= obj).get()
            print(data)
            data = db.reference('Devices/'+rdata[0]+'/Data/Temperature_logs', app= obj).child(str(data-1)).get();
            sdata = "\nDate = "+str(data["Date"])+"\nTime = "+str(data["Time"]) +"\nTemperature = "+str(round(data["Temperature"],1))+"F\n"
            #print("data rec - ")
            #sdata = "\nDate    Time    Temperature\n"
            #for i in data:
            #    sdata += str(i["Date"]) +"  "+ str(i["Time"]) +"  "+ str(round(i["Temperature"],1))+"F\n"
            print(data)
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="Last temperature reading data of "+rdata[1]+" device is - "+str(sdata),
                              message_id=call.message.message_id,
                              )
        except Exception as e:
            bot.answer_callback_query(callback_query_id=call.id,
                              show_alert=True,
                              text="[ERROR] "+str(e))

    if (call.data.startswith("[[--GET-POR ")):
        print(call.data[12:])
        rdata = call.data[12:].split(",")
        try:
            data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Pulse_Oximeter_logs_CIV', app= obj).get()
            data = db.reference('Devices/'+rdata[0]+'/Data/Pulse_Oximeter_logs', app= obj).child(str(data-1)).get();
            sdata = "\nDate = "+str(data["Date"])+"\nTime = "+str(data["Time"]) +"\nHeart rate = "+str(round(data["Heart rate"],1))+"\nSPO2 = "+str(data["Spo2"])+"\n"
            print(data)
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="Last Pulse oximetry data of "+rdata[1]+" device is - "+str(sdata),
                              message_id=call.message.message_id,
                              )
        except Exception as e:
            bot.answer_callback_query(callback_query_id=call.id,
                              show_alert=True,
                              text="[ERROR] "+str(e))

    if (call.data.startswith("[[--GET-NOTES ")):
        print(call.data[14:])
        rdata = call.data[14:].split(",")
        try:
            data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Messages_logs_CIV', app= obj).get()
            print(data,rdata[0],rdata[1])
            mess = db.reference('Devices/'+rdata[0]+'/Data/Messages_logs/'+str(int(data)-1), app= obj).get()
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="last message note for "+rdata[1]+" device is - \n"+mess['Content']+'\n\n'+"Date : "+mess['Date']+'\n'+"Time : "+mess['Time'],
                              message_id=call.message.message_id,
                              )
        except Exception as e:
            bot.answer_callback_query(callback_query_id=call.id,
                              show_alert=True,
                              text="[ERROR] "+str(e))



    if (call.data.startswith("[[--SET-MESSAGE ")):
        #print(call.data[16:])
        print("[DEBUG] ")
        rdata = call.data[16:].split(",")
        msg_data = buffer.pop(rdata[2])
        try:
            cv_data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Messages_logs_CIV', app= obj).get()
            su_data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Stream_Update', app= obj).get()
            su_data = str(su_data)
            su_data = int(su_data[1:])+1
            ctime = datetime.now(ist1)
            db.reference('Devices/'+rdata[0]+'/Data/Messages_logs/'+str(cv_data), app= obj).set({
                            "Date":ctime.strftime("%-d-%-m-%Y"),
                            "Time":ctime.strftime("%H-%M-%S"),
                            "Content":msg_data,
                            "By":{"Telegram": {"name":str(call.from_user.first_name),
                            "username":str(call.from_user.username),
                            "chat id":str(call.from_user.id)}}})
            db.reference('Devices/'+rdata[0]+'/Data/Metadata/Messages_logs_CIV', app= obj).set(int(cv_data)+1)
            db.reference('Devices/'+rdata[0]+'/Data/Metadata/Stream_Update', app= obj).set("M"+str(su_data))
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="Message set to device "+str(rdata[1])+" successfully  (note : write message/note content in the end after 'content' keyword)",
                              message_id=call.message.message_id)

        except Exception as e:
            print("[ERROR] "+str(e))

    if (call.data.startswith("[[--SET-ALARM-TIME ")):
        #print(call.data[16:])
        rdata = call.data[19:].split(",")
        strdata = buffer.pop(rdata[2])
        if(strdata[0] == ' '):
            strdata = strdata[1:]
        strdata = re.sub(',,',',', strdata)
        try:
            su_data = db.reference('Devices/'+rdata[0]+'/Data/Metadata/Stream_Update', app= obj).get()
            su_data = str(su_data)
            su_data = int(su_data[1:])+1
            data = db.reference('Devices/'+rdata[0]+'/Data/medicine alarm time', app= obj).set(strdata)
            db.reference('Devices/'+rdata[0]+'/Data/Metadata/Stream_Update', app= obj).set("A"+str(su_data))
            bot.edit_message_text(chat_id=call.message.chat.id,
                              text="Medicine alarm times - \""+strdata+"\" to "+str(rdata[1])+" device set successfully  (note : set alarm times in format - hh:mm:ss or hh:mm )",
                              message_id=call.message.message_id)

        except Exception as e:
            print("[ERROR] "+str(e))

    at_data = re.findall("[^:][0-1][0-9] ?[:-] ?[0-5][0-9] ?[:-] ?[0-5][0-9]|[^:][0-1][0-9] ?[:-] ?[0-5][0-9]|[^:]2[0-3] ?[:-] ?[0-5][0-9] ?[:-] ?[0-5][0-9]|[^:]2[0-3] ?[:-] ?[0-5][0-9]",call.message.text)

    if(at_data):
        bot.edit_message_text(chat_id=call.message.chat.id,
                              text="New alarm times : "+str(at_data)+" set successfully !",
                              message_id=call.message.message_id)


@bot.message_handler(commands=['start',])
def send_welcome(message):
    bot.reply_to(message,"Welcome to health caretaker bot")

@bot.message_handler(commands=['get_id',])
def send_id(message):
    bot.reply_to(message,message.from_user.id)

@bot.message_handler(commands=['get_device_data',])
def send_options(message):
    bot.send_message(chat_id=message.chat.id,
                     text="Select Option",
                     reply_markup=makeOperationsKeyboard(),
                     parse_mode='HTML')




@bot.message_handler(regexp="^[g|G][e|E][t|T][ ]")
def get_data(message):
    try :
        if re.findall("alarm time|medicine time",message.text.lower()):
            User_id = db.reference('Telegram_IDs/'+str(message.from_user.id), app= obj).get()
            #print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]

                if Verf_Dev:
                    markup = types.InlineKeyboardMarkup()
                    for key, value in Verf_Dev.items():
                        markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data="[[--GET-ALARM-TIMES "+key+","+value['Name']))
                    bot.send_message(chat_id=message.chat.id,
                     text="Select device",
                     reply_markup=markup,
                     parse_mode='HTML')
                else:
                    bot.send_message(chat_id=message.chat.id,
                    text="No Devices Found !")

        if re.findall("message|note",message.text.lower()):
            User_id = db.reference('Telegram_IDs/'+str(message.from_user.id), app= obj).get()
            #print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]

                if Verf_Dev:
                    markup = types.InlineKeyboardMarkup()
                    for key, value in Verf_Dev.items():
                        markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data="[[--GET-NOTES "+key+","+value['Name']))
                    bot.send_message(chat_id=message.chat.id,
                     text="Select device",
                     reply_markup=markup,
                     parse_mode='HTML')
                else:
                    bot.send_message(chat_id=message.chat.id,
                    text="No Devices Found !")




    except Exception as e:
        print("[ERROR] "+str(e))

@bot.message_handler(regexp="^[s|S][e|E][t|T]")
def set_data(message):
    try:
        if re.findall("message.+content ?[-:=]?.+|note.+content ?[-:=]?.+",message.text.lower()):
            User_id = db.reference('Telegram_IDs/'+str(message.from_user.id), app= obj).get()
            #print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                #print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]

                if Verf_Dev:
                    data = re.search("message.+content ?[-:=]?.+|note.+content ?[-:=]?.+",message.text.lower())
                    idx = data.span()
                    data = message.text[idx[0]:idx[1]]
                    r = re.search("[c|C][o|O][n|N][t|T][e|E][n|N][t|T] ?[-:=]?",data)
                    idx = r.span()
                    msg_data = data[idx[1]:]
                    markup = types.InlineKeyboardMarkup()
                    buffer_key = str(int(random()*1000000))
                    buffer[buffer_key] = msg_data
                    for key, value in Verf_Dev.items():
                        markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data="[[--SET-MESSAGE "+str(key)+","+value['Name']+","+buffer_key))
                    bot.send_message(chat_id=message.chat.id,
                     text="Select device",
                     reply_markup=markup,
                     parse_mode='HTML')
                else:
                    bot.send_message(chat_id=message.chat.id,
                    text="No Devices Found !")

            else:
                pass
        elif re.findall("alarm time|medicine time",message.text.lower()):
            User_id = db.reference('Telegram_IDs/'+str(message.from_user.id), app= obj).get()
            #print(User_id)
            if User_id:
                Devices = db.reference('Users/'+str(User_id)+'/devices', app= obj).get()
                #print(Devices)
                Verf_Dev = {}
                for d in Devices:
                    #print("[PATH] "+str('Devices/'+d+'/Users/Access/'+User_id))
                    if(db.reference('Devices/'+d+'/Users/Access/'+User_id, app= obj).get() or db.reference('Devices/'+d+'/Users/Admin', app= obj).get() == User_id):
                        Verf_Dev[d] = Devices[d]

                if Verf_Dev:
                    data = re.findall("[^:][0-1][0-9] ?[:-] ?[0-5][0-9] ?[:-] ?[0-5][0-9]|[^:][0-1][0-9] ?[:-] ?[0-5][0-9]|[^:]2[0-3] ?[:-] ?[0-5][0-9] ?[:-] ?[0-5][0-9]|[^:]2[0-3] ?[:-] ?[0-5][0-9]",message.text)
                    strdata= ""
                    for i in data:
                        strdata += str(i+",")
                    if len(strdata)>3:
                        markup = types.InlineKeyboardMarkup()
                        buffer_key = str(int(random()*1000000))
                        buffer[buffer_key] = strdata
                        for key, value in Verf_Dev.items():
                            markup.add(types.InlineKeyboardButton(text=value['Name'],
                                              callback_data="[[--SET-ALARM-TIME "+str(key)+","+value['Name']+","+buffer_key))
                        bot.send_message(chat_id=message.chat.id,
                            text="Select device",
                            reply_markup=markup,
                            parse_mode='HTML')

                    else:
                        bot.reply_to(message,"[ERROR] could'nt find any valid times in message")
    except Exception as e:
        print("[ERROR] "+str(e))
        bot.reply_to(message,"[ERROR] error accessing path")



bot.infinity_polling()