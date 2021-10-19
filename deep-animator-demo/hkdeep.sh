#!/bin/bash
#set -x
#trap read debug

cd /root

PauseFile=./PAUSE-hk
RemoteDeepAnimator=peter@www.mission-base.com://var/www/mission-base.com/html/tamiko/lendmeyourface/online-hk
LocalDeepAnimator=/data/deep-animator-demo/deep_animator_demo

# Run forever
#
while true; do

  # Remove local file telling us to pause
  #
  rm -f "$PauseFile"

  # Copy the file from the remote host, if a user deleted it, we start working
  #
  scp "$RemoteDeepAnimator/$PauseFile" .

  # Check if the file still existed on the remote host and has been copied
  #
  if test -f "$PauseFile"; then

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
  rsync -avz --no-perms --rsh='ssh -p22' "$RemoteDeepAnimator/drivingVideos/" "$LocalDeepAnimator/online-hk/drivingVideos"

  # Fetch all remote merge videos
  #
  rsync -avz --no-perms --rsh='ssh -p22' "$RemoteDeepAnimator/mergeVideos/" "$LocalDeepAnimator/online-hk/mergeVideos"
  
  # Run forever on all videos
  #
  while true; do
  
    # No video created yet for any video
    #
    CreatedNewVideoForAnyVideo='false'
    echo "CreatedNewVideoForAnyVideo=$CreatedNewVideoForAnyVideo"
    
    # For all existing input videos
    #
    for Video in "$LocalDeepAnimator/online-hk/drivingVideos/"*; do
      
      MergeInput="$Video"
      MergeInput="${MergeInput/256.mp4/512.mp4}"
      MergeInput="${MergeInput/drivingVideos/mergeVideos}"
      echo "MergeInput $MergeInput"
      
      MergeInput1080="${MergeInput/512.mp4/1080.mp4}"
      echo "MergeInput1080 $MergeInput1080"
      
      if test -f "$MergeInput1080" ; then

        echo "MergeInput1080 $MergeInput1080 exists, nothing to todo"
      else
      
        echo ''
        echo "MergeInput1080 $MergeInput1080 missing, creating it"
        echo ''
        echo "RUNNING >> ffmpeg -i $MergeInput -vf scale=1080:1080 $MergeInput1080"
        echo ''
        ffmpeg -i "$MergeInput" -vf scale=1080:1080 "$MergeInput1080"
        echo ''
        
      fi

      MergeInputPadded="${MergeInput/512.mp4/Padded.mp4}"
      echo "MergeInputPadded $MergeInputPadded"
      
      if test -f "$MergeInputPadded" ; then

        echo "MergeInputPadded $MergeInputPadded exists, nothing to todo"
      else
      
        echo ''
        echo "MergeInputPadded $MergeInputPadded missing, creating it"
        echo ''
        echo "RUNNING >> ffmpeg $MergeInput1080 -vf pad=width=1920:height=1080:x=420:y=0:color=black $MergeInputPadded"
        echo ''
        ffmpeg -i "$MergeInput1080" -vf pad=width=1920:height=1080:x=420:y=0:color=black "$MergeInputPadded"
        echo ''
        
      fi
      
      # Generate the name of the result video
      #
      Video=$(basename "$Video")
      VideoName="${Video%.*}" 
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
        echo "CreatedNewVideoForVideo=$CreatedNewVideoForVideo"
    
        # Fetch all remote input images
        #
        rsync -avz --no-perms --rsh='ssh -p22' "$RemoteDeepAnimator/inputPictures/*.png" "$LocalDeepAnimator/online-hk/inputPictures"
        
        # For all existing input images, newest first
        #
        for Picture in `ls -r "$LocalDeepAnimator/online-hk/inputPictures/"`; do

          Picture=$(basename "$Picture")
          PictureName="${Picture%.*}"
          echo "PictureName=$PictureName"

          if [ "$PictureName" = 'index' ] || [ "$PictureName" = '*' ]; then
            continue
          fi

          Result="${PictureName}_${VideoName}.mp4"
          ResultTxt="${PictureName}_${VideoName}.txt"
          ResultVideo="$LocalDeepAnimator/online-hk/generatedVideos/$Result"
          ResultVideo1080="${ResultVideo/.mp4/_1080.mp4}"
          ResultVideoPadded="${ResultVideo/.mp4/_Padded.mp4}"
          MergedVideo1="$LocalDeepAnimator/online-hk/mergedVideos/1-$Result"
          MergedVideo="$LocalDeepAnimator/online-hk/mergedVideos/$Result"
          CreatedVideo="$LocalDeepAnimator/online-hk/createdVideos/$Result"
          MergedVideoTxt="$LocalDeepAnimator/online-hk/mergedVideos/$ResultTxt"
        
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

          echo "MergedVideo $MergedVideo missing, creating it"
          echo "ResultVideo $ResultVideo"
          echo "ResultVideo1080 $ResultVideo1080"
          echo "ResultVideoPadded $ResultVideoPadded"
          echo ""
          rm -f "$ResultVideo"
          rm -f "$ResultVideo1080"
          rm -f "$ResultVideoPadded"
          echo ""
          
          # Determine the previous result video
          #
          PreviousResultVideo=""
          for PreviousResult in `ls -r "$LocalDeepAnimator/online-hk/generatedVideos/" | grep Padded | grep "_${VideoName}"`; do

            PreviousResult=$(basename "$PreviousResult")
            PreviousResult="${PreviousResult%.*}"
            PreviousResultVideo="${LocalDeepAnimator}/online-hk/generatedVideos/${PreviousResult}.mp4"
            echo "PreviousResultVideo $PreviousResultVideo"
            break
          done
        
          # Create a deep_animator.py script from a template
          #
          sed "s/SOURCE_IMAGE/$Picture/" "$LocalDeepAnimator/hkdeep_animator.template" | sed "s/DRIVING_VIDEO/$Video/" >"$LocalDeepAnimator/deep_animator.py"

          # Run deep animator to create the result video
          #
          echo ''
          echo 'RUNNING DEEP'
          echo ''
          echo "RUNNING >> (cd $LocalDeepAnimator; poetry run deep_animator; cp deep_animator.py.ok deep_animator.py)"
          echo ''
          (cd "$LocalDeepAnimator"; poetry run deep_animator; cp deep_animator.py.ok deep_animator.py)
          echo ''
          echo 'DONE'
          echo ''

          echo ''
          echo "ResultVideo1080 $ResultVideo1080 missing, creating it"
          echo ''
          echo "RUNNING >> ffmpeg -i $ResultVideo -vf scale=1080:1080 $ResultVideo1080"
          echo ''
          ffmpeg -i "$ResultVideo" -vf scale=1080:1080 "$ResultVideo1080"
          echo ''
        
          echo ''
          echo "ResultVideoPadded $ResultVideoPadded missing, creating it"
          echo ''
          echo "RUNNING >> ffmpeg $ResultVideo1080 -vf pad=width=1920:height=1080:x=420:y=0:color=black $ResultVideoPadded"
          echo ''
          ffmpeg -i "$ResultVideo1080" -vf pad=width=1920:height=1080:x=420:y=0:color=black "$ResultVideoPadded"
          echo ''
        
          echo "Removing $MergedVideo1"
          rm -f "$MergedVideo1"
          echo ''
          
          # Use ffmpeg to make one video, the output and the driving video side by side
          #
          echo ''
          echo 'RUNNING MERGE 1'
          echo ''
          echo "RUNNING >> ffmpeg -i $ResultVideoPadded -i $MergeInputPadded -filter_complex hstack $MergedVideo1"
          echo ''
          ffmpeg -i "$ResultVideoPadded" -i "$MergeInputPadded" -filter_complex hstack "$MergedVideo1"
          echo ''
          echo 'DONE'
          echo ''

          # Check if one of the previous result video exists
          #
          if test -f "$PreviousResultVideo"; then

            echo "PreviousResultVideo $PreviousResultVideo exists, using it"
          else
          
            PreviousResultVideo="$ResultVideoPadded"
            echo "Using $PreviousResultVideo as previous"
          fi
          
          # Use ffmpeg to make a triple video, the output and the driving video and the output side by side
          #
          echo ''
          echo 'RUNNING MERGE 2'
          echo ''
          echo "RUNNING >> ffmpeg -i $MergedVideo1 -i $PreviousResultVideo -filter_complex hstack $MergedVideo"
          echo ''
          ffmpeg -i "$MergedVideo1" -i "$PreviousResultVideo" -filter_complex hstack "$MergedVideo"
          echo ''
          echo 'DONE'
          echo ''

          echo "Removing $MergedVideo1"
          rm -f "$MergedVideo1"
          echo ''

          # Upload the video to the server
          #
          echo "Uploading video $LocalDeepAnimator/online-hk/mergedVideos/ to $RemoteDeepAnimator/outputVideos/"
          rsync -avz --no-perms --rsh='ssh -p22' "$LocalDeepAnimator/online-hk/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"
          echo ''

          # Create an empty text file
          #
          touch "$MergedVideoTxt"

          # Upload the empty text file to the server
          #
          echo "Uploading text $LocalDeepAnimator/online-hk/mergedVideos/ to $RemoteDeepAnimator/outputVideos/"
          rsync -avz --no-perms --rsh='ssh -p22' "$LocalDeepAnimator/online-hk/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"

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
  rsync -avz --no-perms --rsh='ssh -p22' "$LocalDeepAnimator/online-hk/mergedVideos/" "$RemoteDeepAnimator/outputVideos/"

  # Move all created videos to a local directory
  #
  mv "$LocalDeepAnimator/online-hk/mergedVideos/"* "$LocalDeepAnimator/online-hk/createdVideos/"
    
  # No video was created at all
  #    
  if [ "$CreatedNewVideo" != 'true' ]; then
    
    # For all existing input images
    #
    for Picture in "$LocalDeepAnimator/online-hk/inputPictures/"*; do

      Picture=$(basename "$Picture")
      PictureName="${Picture%.*}"
      echo "PictureName=$PictureName"

      if [ "$PictureName" = 'index' ] || [ "$PictureName" = '*' ]; then
        continue
      fi

      # Create a text file indicating we handled the picture
      #
      PictureNameTxt="${LocalDeepAnimator}/online-hk/handledPictures/${PictureName}.txt"
      touch "$PictureNameTxt"

      # Copy the file to server
      #
      scp "$PictureNameTxt" "$RemoteDeepAnimator/handledPictures/"
    
    # done for all existing input images
    #
    done
    
    # Move handled pictures into the handledPictures directory
    #
    mv "$LocalDeepAnimator/online-hk/inputPictures/"* "$LocalDeepAnimator/online-hk/handledPictures/"
    
    # Copy a PauseFile to the server, in the next loop we delay unless some user deleted it
    #
    touch "$PauseFile"
    scp "$PauseFile" "$RemoteDeepAnimator/$PauseFile"
  
  fi

# end forever
#
done

