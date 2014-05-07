Strict
Import vsat.foundation
Import mojo


Class Audio
	
	Function PlaySound:Int(sound:Sound, channel:Int = 0, volume:Float = 1.0)
		#If TARGET = "html5"
			Return 0
		#End
		If Not sound Return 0
		Local i:Int
		If channel = 0
			For i = 0 Until 32
				If ChannelState(i) = 0
					Exit
				End
			End
		Else
			i = channel
		End
		mojo.audio.PlaySound(sound, i)
		SetChannelVolume(i, volume)
		Return i
	End
	
	Function StopAllSounds:Void()
		Local i:Int
		For i = 0 Until 32
			StopChannel(i)
		Next
	End
	
	Function GetSound:Sound(path:String)
		If SoundCache.Contains(path)
			Return SoundCache.Get(path)
		Else
			Local sound:Sound = LoadSound(path)
			If (Not sound) Return Null
			SoundCache.Set(path, sound)
			Return sound
		End
	End
	
	Function DiscardSound:Void(sound:Sound)
		If sound
			SoundCache.Remove(path)
			sound.Discard()
		End
	End
	
	
	Private
	Global musicVolume:Float = 1.0
	Global SoundCache:StringMap<Sound> = New StringMap<Sound>
End

