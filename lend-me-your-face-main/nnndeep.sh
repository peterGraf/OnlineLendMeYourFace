#!/bin/bash
#set -x
#trap read debug

cd /home/peter/lend-me-your-face-main/

#Minute=""
PauseFile=./NNN_PAUSE
RemoteDeepAnimator=/var/www/html/lendmeyourface/online-nnn
LocalDeepAnimator=/home/peter/lend-me-your-face-main/data/online-nnn

# Run forever
#
while true; do

  # Remove local file telling us to pause
  #
  rm -f "$PauseFile"

  # Copy the file from the remote host, if a user deleted it, we start working
  #
  echo scp "$RemoteDeepAnimator/$PauseFile" .
  scp "$RemoteDeepAnimator/$PauseFile" .

  # Check if the file still existed on the remote host and has been copied
  #
  if test -f "$PauseFile"; then

    #NewMinute="`date +"%M"`"
    #echo "NewMinute $NewMinute"

    #if [[ "$Minute" == "$NewMinute" ]]; then
    #    echo ""
    #else
        #echo "odeep pausing at `date +"%Y-%m-%d %H:%M:%S"`" > /tmp/odeepRunning.txt
        #scp /tmp/odeepRunning.txt peter@www.mission-base.com:/var/www/mission-base.com/html/stats/
        #Minute=$NewMinute
        #echo "Minute $Minute"
    #fi

    # PauseFile exists, delay
    #
    echo "`date +"%Y-%m-%d %H:%M:%S"` $PauseFile exists."
    sleep 3
    continue
  fi

  # PauseFile did not exist, start working after one second
  #
  sleep 1
  echo "`date +"%Y-%m-%d %H:%M:%S"` $PauseFile does not exist, running deep."

  # No video created yet
  #
  CreatedNewVideo='false'

  # Fetch all remote input videos
  #
  rsync -av "$RemoteDeepAnimator/drivingVideos/" "$LocalDeepAnimator/drivingVideos"

  # Fetch all remote merge videos
  #
  rsync -av "$RemoteDeepAnimator/mergeVideos/" "$LocalDeepAnimator/mergeVideos"
  
  # Run forever on all videos
  #
  while true; do
  
    # No video created yet for any video
    #
    CreatedNewVideoForAnyVideo='false'
    
    # For all existing input videos
    #
    for Video in "$LocalDeepAnimator/drivingVideos/"*; do
      
      MergeInput="$Video"
      MergeInput="${MergeInput/256.mp4/512.mp4}"
      MergeInput="${MergeInput/drivingVideos/mergeVideos}"
      echo "MergeInput $MergeInput"

      MergeInput640="${MergeInput/512.mp4/640.mp4}"
      echo "MergeInput640 $MergeInput640"
      
      if test -f "$MergeInput640" ; then

        echo "MergeInput640 $MergeInput640 exists, nothing to todo"
      else
      
        echo ''
        echo "MergeInput640 $MergeInput640 missing, creating it"
        echo ''
        echo "RUNNING >> ffmpeg -i $MergeInput -vf scale=640:640 $MergeInput640"
        echo ''
        /home/peter/miniconda3/bin/ffmpeg -i "$MergeInput" -vf scale=640:640 "$MergeInput640"
        echo ''
        
      fi
	  
      # Generate the name of the result video
      #
      Video=$(basename "$Video")
      VideoName="${Video%.*}" 
      echo ""
      echo "VideoName=$VideoName"
      
      if [ "$VideoName" = 'index' ] || [ "$VideoName" = '*' ]; then
         continue
      fi

      # Run forever on video
      #
      while true; do
    
        # No video created yet for this video
        #
        CreatedNewVideoForVideo='false'
    
        # Fetch all remote input images
        #
        rsync -av "$RemoteDeepAnimator/inputPictures/" "$LocalDeepAnimator/inputPictures"

        # For all existing input images, newest first
        #
        for Picture in `ls -r "$LocalDeepAnimator/inputPictures/"`; do

          Picture=$(basename "$Picture")
          PictureName="${Picture%.*}"
          #echo "PictureName=$PictureName"

          if [ "$PictureName" = 'index' ] || [ "$PictureName" = '*' ]; then
            continue
          fi

          Result="${PictureName}_${VideoName}.mp4"
          ResultTxt="${PictureName}_${VideoName}.txt"
          ResultVideo="$LocalDeepAnimator/generatedVideos/$Result"
		  ResultVideo640="${ResultVideo/.mp4/_640.mp4}"
          MergedVideo1="$LocalDeepAnimator/mergedVideos/1-$Result"
          MergedVideo="$LocalDeepAnimator/mergedVideos/$Result"
          CreatedVideo="$LocalDeepAnimator/createdVideos/$Result"
          MergedVideoTxt="$LocalDeepAnimator/mergedVideos/$ResultTxt"
        
          # Check if one of the result videos exists
          #
          if test -f "$MergedVideo"; then

            echo "MergedVideo $MergedVideo exists, nothing to todo"
            continue
          fi

          if test -f "$CreatedVideo" ; then

            echo "CreatedVideo $CreatedVideo exists, nothing to todo"
            continue
          fi

          echo "MergedVideo $MergedVideo missing, creating it!!!!!"
		  echo "ResultVideo $ResultVideo"
          echo "ResultVideo640 $ResultVideo640"
          echo ""
          rm -f "$ResultVideo"
		  rm -f "$ResultVideo640"
          echo ""
          
		  # Determine the previous result video
          #
          PreviousResultVideo=""
          for PreviousResult in `ls -r "$LocalDeepAnimator/generatedVideos/" | grep _640.mp4 | grep "_${VideoName}"`; do

            PreviousResult=$(basename "$PreviousResult")
            PreviousResult="${PreviousResult%.*}"
            PreviousResultVideo="${LocalDeepAnimator}/generatedVideos/${PreviousResult}.mp4"
            echo "PreviousResultVideo $PreviousResultVideo"
            break
          done
		  
          # Run fake yourself to create the result video
          #
          (poetry run python fake-yourself.py --source_image $LocalDeepAnimator/inputPictures/$Picture --driving_video $LocalDeepAnimator/drivingVideos/$Video --result_path $LocalDeepAnimator/generatedVideos --size 512)

          echo ''
          echo "ResultVideo640 $ResultVideo640 missing, creating it"
          echo ''
          echo "RUNNING >> ffmpeg -i $ResultVideo -vf scale=640:640 $ResultVideo640"
          echo ''
          /home/peter/miniconda3/bin/ffmpeg -i "$ResultVideo" -vf scale=640:640 "$ResultVideo640"
          echo ''
		  
		  echo "Removing $MergedVideo1"
          rm -f "$MergedVideo1"
          echo ''
		  
          # Use ffmpeg to make one video, the output and the driving video side by side
          #
		  echo 'RUNNING MERGE 1'
          echo ''
          /home/peter/miniconda3/bin/ffmpeg -i "$ResultVideo640" -i "$MergeInput640" -filter_complex hstack "$MergedVideo1"

          # Check if one of the previous result video exists
          #
          if test -f "$PreviousResultVideo"; then

            echo "PreviousResultVideo $PreviousResultVideo exists, using it"
          else
          
            PreviousResultVideo="$ResultVideo640"
            echo "Using $PreviousResultVideo as previous"
          fi
		  
          # Use ffmpeg to make a triple video, the output and the driving video and the output side by side
          #
          echo ''
          echo 'RUNNING MERGE 2'
          echo ''
          echo "RUNNING >> ffmpeg -i $MergedVideo1 -i $PreviousResultVideo -filter_complex hstack $MergedVideo"
          echo ''
          /home/peter/miniconda3/bin/ffmpeg -i "$MergedVideo1" -i "$PreviousResultVideo" -filter_complex hstack "$MergedVideo"
          echo ''
          echo 'DONE'
          echo ''

          echo "Removing $MergedVideo1"
          rm -f "$MergedVideo1"
          echo ''
		  
          # Upload the video to the server
          #
          rsync -av "$LocalDeepAnimator/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"

          # Create an empty text file
          #
          touch "$MergedVideoTxt"

          # Upload the empty text file to the server
          #
          rsync -av "$LocalDeepAnimator/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"

          # let the server now we are still running
          #
          #echo "odeep running at `date +"%Y-%m-%d %H:%M:%S"`" > /tmp/odeepRunning.txt
          #scp /tmp/odeepRunning.txt peter@www.mission-base.com:/var/www/mission-base.com/html/stats/

          # Break to Run forever on all videos
          #
          CreatedNewVideo='true'
          CreatedNewVideoForVideo='true'
          CreatedNewVideoForAnyVideo='true'
          break 3
          
        # end for Picture
        #
        done
      
        # Not a single video was created for the video
        #
        if [ "$CreatedNewVideoForVideo" != 'true' ]; then
          break
        fi
       
      # end run forever on video
      #
      done

    # end for all existing input videos
    #
    done

    # Not a single video was created for any video
    #
    if [ "$CreatedNewVideoForAnyVideo" != 'true' ]; then
      break
    fi
        
  # end run on all videos
  #
  done
  
  # Sync all result videos to the server
  #
  rsync -av "$LocalDeepAnimator/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"

  # Move all created videos to a local directory
  #
  mv "$LocalDeepAnimator/mergedVideos/"* "$LocalDeepAnimator/createdVideos/"
    
  # No video was created at all
  #    
  if [ "$CreatedNewVideo" != 'true' ]; then
    
    # For all existing input images
    #
    for Picture in "$LocalDeepAnimator/inputPictures/"*; do

      Picture=$(basename "$Picture")
      PictureName="${Picture%.*}"
      echo "PictureName=$PictureName"

      if [ "$PictureName" = 'index' ] || [ "$PictureName" = '*' ]; then
        continue
      fi

      # Create a text file indicating we handled the picture
      #
      PictureNameTxt="${LocalDeepAnimator}/handledPictures/${PictureName}.txt"
      touch "$PictureNameTxt"

      # Copy the file to server
      #
      scp "$PictureNameTxt" "$RemoteDeepAnimator/handledPictures/"
    
    # done for all existing input images
    #
    done
    
    # Move handled pictures into the handledPictures directory
    #
    mv "$LocalDeepAnimator/inputPictures/"* "$LocalDeepAnimator/handledPictures/"
    
    # Copy a PauseFile to the server, in the next loop we delay unless some user deleted it
    #
    touch "$PauseFile"
    scp "$PauseFile" "$RemoteDeepAnimator/$PauseFile"
  
  fi

# end forever
#
done

