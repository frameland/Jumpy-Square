Strict
Import vsat
Import game
Import medals
Import defaultlang


Function SaveGame:Void()
	Local saveString:String
	saveString += "highscore = " + GameScene.Highscore + "~n"
	saveString += "isMuted = " + Int(Audio.IsMuted()) + "~n"
	saveString += "lang = " + Localize.GetCurrentLanguage() + "~n"
	saveString += Medals.KeyValues()
	SaveState(saveString)
End

Function LoadGame:Void()
	Local state:String = LoadState()
	
	Local keyValues:KeyValuePair = New KeyValuePair
	Local error:VError = New VError
	Local success:Bool = keyValues.InitWithString(state, error)
	If success
		GameScene.Highscore = keyValues.GetInt("highscore")
		Medals.InitWithKeyValuePair(keyValues)
	Else
		Print error.message
	End
	
	Local language:String = keyValues.GetString("lang", "")
	If language = ""
		language = Locale.GetDefaultLanguage()
		If language <> "en" And language <> "de"
			language = "en"
		End
	End
	Localize.SetLanguage(language)
	
	If keyValues.GetBool("isMuted", False)
		Audio.Mute()
	End
End
