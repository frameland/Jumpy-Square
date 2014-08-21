Strict
Import gameoverstate

Class AdventureGameover Extends GameOverState
	
	Method New(adventure:AdventureScene)
		Self.adventure = adventure
		font = FontCache.GetFont(RealPath("font"))
		MedalItem.font = font
		
		bgY = Vsat.ScreenHeight * 0.15 + font.height * 1.5
		bgHeight = Vsat.ScreenHeight * 0.3
		
		playAgainText = Localize.GetValue("gameover_play_again")
		earnedMedalsText = Localize.GetValue("gameover_earned_medals")
		
		glowImage = ImageCache.GetImage(RealPath("glow.png"))
		glowImage.SetHandle(0, glowImage.Height()/2)
		
		newHighscore = New VLabel(Localize.GetValue("gameover_new_highscore"))
		newHighscore.SetFont(font)
		newHighscore.color.Set(Color.Yellow)
		newHighscore.color.Alpha = 0.0
		newHighscore.alignHorizontal = AngelFont.ALIGN_RIGHT
		newHighscore.alignVertical = AngelFont.ALIGN_CENTER
		newHighscore.position.Set(Vsat.ScreenWidth * 0.9, adventure.backButton.position.y + Vsat.ScreenHeight * 0.01)
		
		Local baseUnit:Float = Vsat.ScreenWidth * 0.5
		Local size:Int = Int(baseUnit * 0.01)
		newHighscoreEffect = New ParticleEmitter
		newHighscoreEffect.InitWithSize(30)
		newHighscoreEffect.particleLifeSpan = 1.0
		newHighscoreEffect.position.Set(newHighscore.position.x - newHighscore.size.x/2, 0)
		newHighscoreEffect.positionVariance.Set(newHighscore.size.x/2, 0)
		newHighscoreEffect.size.Set(size, size)
		newHighscoreEffect.endSize.Set(size, size)
		newHighscoreEffect.speed = baseUnit * 0.4
		newHighscoreEffect.speedVariance = baseUnit * 0.2
		newHighscoreEffect.endColor.Alpha = 0.0
		newHighscoreEffect.emissionAngle = 90
		newHighscoreEffect.oneShot = True
		newHighscoreEffect.additiveBlend = True
	End
	
	Method Update:Void(dt:Float)
		If Not active Return
		
		time += dt
		If adventure.UsedActionKey() And adventure.backButton.isDown = False
			If adventure.backgroundColor.Equals(adventure.gameOverColor)
				If TouchY() > Vsat.ScreenHeight * 0.7
					ReturnToGame()
				End
			End
		End
		
		VAction.UpdateList(actions, dt)
		UpdateSwiping(dt)
		newHighscoreEffect.Update(dt)
	End
	
	Private
	Field adventure:AdventureScene
End