#!/bin/python3
import os
from time import sleep
import logging as log
import curses
import configparser

log.basicConfig (level=log.INFO)
config = configparser.ConfigParser()
config.read('config.ini')

def getpath (path):
    path = os.path.normpath (path)
    if not os.path.isfile (path):
        raise FileNotFoundError (os.path.abspath (path))
    return path

def select_option(stdscr, title, text, options, descriptions):
    curses.curs_set(0)  # Скрываем курсор
    current_option = 0

    while True:
        stdscr.clear()

        # Добавляем заголовок и текст
        stdscr.addstr(1, 1, title, curses.A_BOLD)
        stdscr.addstr(3, 1, 'Используйте стрелки вверх/вниз для навигации и Enter для выбора.', curses.A_ITALIC)
        
        # Выводим список опций
        for i, option in enumerate(options):
            if i == current_option:
                stdscr.addstr(i + 5, 1, f'→ {option} - {descriptions[i]}', curses.A_BOLD)
            else:
                stdscr.addstr(i + 5, 1, f'  {option} - {descriptions[i]}')
        
        stdscr.addstr(i + 7, 1, text, curses.A_ITALIC)
        stdscr.refresh()

        # Получаем ввод пользователя
        key = stdscr.getch()

        # Обрабатываем ввод пользователя
        if key == curses.KEY_UP:
            current_option = (current_option - 1) % len(options)
        elif key == curses.KEY_DOWN:
            current_option = (current_option + 1) % len(options)
        elif key == curses.KEY_ENTER or key in [10, 13]:
            selected_option = options[current_option]
            return selected_option

try:
    uhd_quality = config['DEFAULT']['uhd_quality']
    draw_text = config['DEFAULT']['draw_text']
    log.info (" > Загружены опции из файла конфигурации --- ")
except KeyError: 
    uhd_quality = curses.wrapper(select_option, 'Выберите разрешение итогового видео', \
    'Формат WEBM обеспечивает более высокое качество, но обрабатывается медленнее и поддерживается не всеми платформами.', \
    ['0', '1'], \
    ['FullHD MP4', '4K WEBM'])
    draw_text = curses.wrapper(select_option, 'Добавить имя файла в начале видео', \
    'В начале видео будет добавлен текст с названием трека.', \
    ['0', '1'], \
    ['Нет', 'Да'])
    
    if int (curses.wrapper(select_option, 'Сохранить эти настройки?', \
    'Будет создан файл config.ini, в который будут записаны выбранные выше настройки.', \
    ['0', '1'], \
    ['Нет', 'Да'])):
        config['DEFAULT']['uhd_quality'] = uhd_quality
        config['DEFAULT']['draw_text'] = draw_text
        with open ('config.ini', 'w') as configfile:
            config.write (configfile)

is_nix = not (os.name == 'nt')
if is_nix:
    log.warning ("Вы используете ОС Linux, убедитесь, что у вас установлен Wine для запуска exe файлов и даны права на их исполнение (chmod +x *.exe)")

video_res, encoder_settings, outfile_ext = \
['3840x2160', "-pix_fmt yuv420p -c:v libvpx-vp9 -crf 23 -row-mt 1 -c:a libopus -b:a 256k", "webm"] \
if int (uhd_quality) else \
['1920x1080', "-pix_fmt yuv420p -c:v libx264 -profile:v high -preset slow -crf 18 -g 30 -bf 2 -c:a aac -profile:a aac_low -b:a 384k -movflags faststart", "mp4"]
drawtext = ",drawtext=text=%s:fontcolor=whitesmoke:fontsize=(h/20):x='(w-text_w)/2':y='0.9*h':alpha='if(lt(t,0),0,if(lt(t,2),(t-0)/2,if(lt(t,8),1,if(lt(t,10),(2-(t-8))/2,0))))':shadowx=2:shadowy=2" if int (draw_text) else ""

conversion = f"""
ffmpeg -hide_banner \
-f rawvideo -vcodec rawvideo -s 240x135 -pix_fmt rgba -r 60 -i spectrum.raw \
-f rawvideo -vcodec rawvideo -s 240x135 -pix_fmt rgba -r 60 -i background.raw \
-f lavfi -i movie=background.png:loop=0,setpts=N/FRAME_RATE/TB,fps=60,format=rgba \
-f lavfi -i color=s={video_res}:r=60:c=black,format=rgba \
-i in.wav \
-filter_complex "[0:v]scale=240x270:flags=bilinear,scale=480x270:flags=neighbor,scale={video_res}:flags=lanczos[tmp_s];[1:v]scale={video_res}:flags=bicubic[tmp_b];[3:v][tmp_b]overlay=0:0,format=rgba[tmp_b];[2:v][tmp_b]blend=all_mode=multiply,format=rgba[tmp_b];[tmp_b][tmp_s]overlay=0:0{drawtext}" \
{encoder_settings} -shortest output.{outfile_ext}
"""

try:
    log.debug ('Checking that all components are existing')
    iir_processor = getpath ('Audio_IIR_Processing.exe')
    visualizer = getpath ('Spectrum_Visualization.exe')
    
    for filename in os.listdir ('src/music'):
        log.debug ('Checking and setting fanart and source track path')
        trackname = os.path.splitext (filename) [0]
        fanart_path = getpath (f'src/fanart/{trackname}.png')
        srcaudio_path = getpath (f'src/music/{filename}')
        
        log.info (f' > Подготовка фанарта для {filename} --- ')
        
        log.debug ('Adjusting the video resolution...')
        os.system (f'ffprobe -v error -select_streams v -show_entries stream=width,height -of csv=p=0:s=x "{fanart_path}" > fanart_res.txt')
        fanart_res = getpath ("fanart_res.txt")
        
        with open (fanart_res) as restext:
            fanart_width, fanart_height = map (int, restext.read ().split ("x"))
        
        if fanart_width / fanart_height < 16 / 9:
            log.debug ('provided image is vertical')
            os.system (f'ffmpeg -hide_banner -i "{fanart_path}" -filter_complex "[0:v]scale=-1:{video_res.split ("x")[1]}:flags=lanczos,split=2[src][fg];[src]scale=ih*16/9:-1,crop=h=iw*9/16,boxblur=100[bg];[bg][fg]overlay=(W-w)/2:(H-h)/2" background.png')
        elif fanart_width / fanart_height > 16 / 9:
            log.debug ('provided image is horizontal')
            os.system (f'ffmpeg -hide_banner -i "{fanart_path}" -filter_complex "[0:v]scale={video_res.split ("x")[0]}:-1:flags=lanczos,split=2[src][fg];[src]scale=-1:iw*9/16,crop=w=ih*16/9,boxblur=100[bg];[bg][fg]overlay=(W-w)/2:(H-h)/2" background.png')
        else:
            log.debug ('the aspect ratio of provided image is the same as we need, just rescaling')
            os.system (f'ffmpeg -hide_banner -i "{fanart_path}" -vf "scale={video_res}" background.png')
            
        log.debug ('Checking that ffmpeg outputs background.png and that it exists')
        background_path = getpath ('background.png')
        
        log.info (f' > Подготовка файла {filename} --- ')
        os.system (f'ffmpeg -hide_banner -i "{srcaudio_path}" -c:a pcm_s16le -ar 48000 -ac 2 -map_metadata -1 -fflags +bitexact in.wav')
        log.debug ('Checking that ffmpeg outputs in.wav and that it exists')
        in_wav_path = getpath ('in.wav')
        
        log.info (f' > Выполняю преобразование файла {filename} --- ')
        os.system (f'{"./" if is_nix else ""}{iir_processor}')
        log.debug ('Checking that Audio_IIR_Processing.exe outputs out.wav and that it exists')
        out_wav_path = getpath ('out.wav')
        
        log.info (f' > Выполняю построение спектра {filename} --- ')
        os.system (f'{"./" if is_nix else ""}{visualizer}')
        log.debug ('Checking that Spectrum_Visualization.exe outputs spectrum and background raw files and that they exist')
        background_raw_path, spectrum_raw_path = getpath ('background.raw'), getpath ('spectrum.raw')
        
        log.info (f' > Выполняю сборку видео {filename} --- ')
        os.system (conversion % trackname [:64] if draw_text else conversion)
        log.debug ('Checking that ffmpeg outputs final video and that it exists')
        final_video_path = getpath (f'output.{outfile_ext}')
        
        log.info (f' > Очистка временных файлов --- ')
        outdir = os.path.normpath (f'out/{trackname}')
        outfile = os.path.normpath (f'{outdir}/output.{outfile_ext}')
        
        if not os.path.exists (outdir):
            log.debug (f'{outdir} doesnt exist, creating it')
            os.makedirs (outdir)
        log.debug (f'moving output.{outfile_ext} to {outfile}')
        os.rename (final_video_path, outfile)
        for file in (background_path, in_wav_path, out_wav_path, background_raw_path, spectrum_raw_path, fanart_res):
            log.debug (f'removing {file}')
            os.remove (file)
        sleep (10) # waiting until OS will be released
        
except FileNotFoundError:
    log.error ('Ошибка: файл не существует. Вероятно, процесс обработки завершился с ошибкой.')
    raise
except OSError:
    log.error (f'Операция не удалась.')
    raise
else:
    log.info (' > Все файлы успешно обработаны! --- ')
