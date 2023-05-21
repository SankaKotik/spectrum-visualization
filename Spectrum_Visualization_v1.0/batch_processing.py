#!/bin/python3
import os
from time import sleep
import logging as log

log.basicConfig (level=log.INFO)

def getpath (path):
    path = os.path.normpath (path)
    if not os.path.isfile (path):
        raise FileNotFoundError (os.path.abspath (path))
    return path

is_nix = not (os.name == 'nt')
if is_nix:
    log.warning ("Вы используете ОС Linux, убедитесь, что у вас установлен Wine для запуска exe файлов и даны права на их исполнение (chmod +x *.exe)")

video_res = '3840x2160' if bool (int (input ('Выберите разрешение итогового видео:\n 0. FullHD\n 1. 4K\nВаш выбор: '))) else '1920x1080'
conversion = f"""
ffmpeg -hide_banner \
-f rawvideo -vcodec rawvideo -s 240x135 -pix_fmt rgba -r 60 -i spectrum.raw \
-f rawvideo -vcodec rawvideo -s 240x135 -pix_fmt rgba -r 60 -i background.raw \
-f lavfi -i movie=background.png:loop=0,setpts=N/FRAME_RATE/TB,fps=60,format=rgba \
-f lavfi -i color=s={video_res}:r=60:c=black,format=rgba \
-i in.wav \
-filter_complex "[0:v]scale=240x270:flags=bilinear,scale=480x270:flags=neighbor,scale={video_res}:flags=lanczos[tmp_s];[1:v]scale={video_res}:flags=bicubic[tmp_b];[3:v][tmp_b]overlay=0:0,format=rgba[tmp_b];[2:v][tmp_b]blend=all_mode=multiply,format=rgba[tmp_b];[tmp_b][tmp_s]overlay=0:0" \
-pix_fmt yuv420p -c:v libx264 -profile:v high -preset slow -crf 18 -g 30 -bf 2 -c:a aac -profile:a aac_low -b:a 384k -shortest -movflags faststart output.mp4
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
        os.system (conversion)
        log.debug ('Checking that ffmpeg outputs final mp4 video and that it exists')
        final_video_path = getpath ('output.mp4')
        
        log.info (f' > Очистка временных файлов --- ')
        outdir = os.path.normpath (f'out/{trackname}')
        outfile = os.path.normpath (f'{outdir}/output.mp4')
        
        if not os.path.exists (outdir):
            log.debug (f'{outdir} doesnt exist, creating it')
            os.makedirs (outdir)
        log.debug (f'moving output.mp4 to {outfile}')
        os.rename (final_video_path, outfile)
        for file in (background_path, in_wav_path, out_wav_path, background_raw_path, spectrum_raw_path, fanart_res):
            log.debug (f'removing {file}')
            os.remove (file)
        sleep (15) # waiting until OS will be released
        
except FileNotFoundError:
    log.error ('Ошибка: файл не существует. Вероятно, процесс обработки завершился с ошибкой.')
    raise
except OSError:
    log.error (f'Операция не удалась.')
    raise
else:
    log.info (' > Все файлы успешно обработаны! --- ')
