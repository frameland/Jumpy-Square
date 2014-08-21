Strict
Import game
Import adventure_gameover


Class AdventureScene Extends GameScene
	
	Method OnInit:Void()
		Vsat.UpdateScreenSize(DeviceWidth(), DeviceHeight())
		
		normalBgColor.Set($312222)
		gameOverColor.Set($020012)
		
		player = New Player
		enemies = New List<Enemy>
		EnemySpawner.Init()
		
		doubleBall = New DoubleBall
		doubleBall.color.Set($ffd000)
		
		backButton = New BackButton
		backButton.SetFont(RealPath("font2"))
		backButton.color.Alpha = 0.0
		
		InitEffects()
		
		scoreFont = FontCache.GetFont(RealPath("font"))
		
		surpriseSound = Audio.GetSound("audio/surprise.mp3")
		
		gameOverState = New AdventureGameover(Self)
		
		globalAlpha.Alpha = 1.0
		
		InitFeeds()
		
		ResetGame()
		FadeInAnimation()
		
		InitAds()
	End
	
	Method OnBackClicked:Void()
		If gameOver
			If Vsat.transition Return
			HideAds()

			AddAction(New VFadeToAlphaAction(globalAlpha, 0.0, 0.5, EASE_OUT_EXPO))
			mainMenuObject.shouldClearScreen = True
			mainMenuObject.currentSite = 2
			mainMenuObject.currentPosX = Vsat.ScreenWidth * (mainMenuObject.currentSite-1)
			Local transition:= New MoveUpTransition(0.7)
			transition.startPoint = Vsat.ScreenHeight
			transition.SetScene(mainMenuObject)
			Vsat.ChangeSceneWithTransition(mainMenuObject, transition)

			Local fadeColor:= New VFadeToColorAction(backgroundColor, mainMenuObject.currentBgColor, 0.5, LINEAR_TWEEN)
			AddAction(fadeColor)
			
			backgroundEffect.SetNormal()
			Local sound:= Audio.GetSound("audio/fadeout.mp3")
			Audio.PlaySound(sound, 2)
		End
	End
	
	Method CheckNewHighscore:Void()
		If score > GameScene.HighscoreAdventure
			GameScene.HighscoreAdventure = score
			gameOverState.NewHighscore()
			'InitGameCenter()
			'SyncGameCenter(HighscoreAdventure)
		End
	End
	
	Method RenderTip:Void()
	End
	
End









