Strict
Import vsat
Import extra
Import player
Import enemy
Import menu
Import medals
Import feed
Import back
Import save
Import flgamecenter
Import doubleball


#If TARGET = "ios"
Import brl.admob
#ADMOB_PUBLISHER_ID="a15364047eb2dce "
#End

Class GameScene Extends VScene Implements VActionEventHandler, ILabelFeedCallback
	
	Const SPAWN_TIME_RANGE:Float = 1.8
	
	Global Highscore:Int = 0
	Global IsUnlocked:Bool = False
	
	Field player:Player
	Field enemies:List<Enemy>
	Field enemyTimer:Float 'gets set in ResetGame()
	
	Field doubleBall:DoubleBall
	Field hyperModeTimer:Float
	Const HYPER_TIME:Float = 15.0
	
	Field score:Int
	Field targetScore:Int
	Field dodged:Int
	
	Field gameOver:Bool
	Field gameOverState:GameOverState
	
	Field surpriseColor:Color = New Color
	Field waitForSpurprise:Bool
	Field lastSurpriseRound:Int
	Field surpriseSound:Sound
	
	Field transitionInDone:Bool = False
	Field isFirstTime:Bool
	
	Field scoreMannedThisRound:Bool
	
	Field normalBgColor:Color = New Color(Color.Navy)
	Field gameOverColor:Color = New Color($001729) '$000b15
	Field backgroundColor:Color = New Color
	
	Field backButton:BackButton
	Field lastTouchDown:Bool
	
	
'--------------------------------------------------------------------------
' * Init
'--------------------------------------------------------------------------
	Method OnInit:Void()
		player = New Player
		enemies = New List<Enemy>
		doubleBall = New DoubleBall
		
		InitEffects()
		
		scoreFont = FontCache.GetFont(RealPath("font"))
		
		surpriseSound = Audio.GetSound("audio/surprise.mp3")
		
		gameOverState = New GameOverState(Self)
		
		backButton = New BackButton
		backButton.SetFont(RealPath("font2"))
		backButton.color.Alpha = 0.0
		
		globalAlpha.Alpha = 1.0
		
		InitFeeds()
		
		ResetGame()
		FadeInAnimation()
		
		randomTip = RandomTip()
		tip = "Tip"
		
		InitAds()
	End
	
	Method FadeInAnimation:Void()
		Local mainMenu:Object = Vsat.RestoreFromClipboard("MainMenu")
		If mainMenu
			Self.mainMenuObject = MainMenu(mainMenu)
			
			Local delay:= New VDelayAction(2.3)
			delay.Name = "TransitionInDone"
			AddAction(delay)
			
			Local transition:= New MoveDownTransition(1.5)
			transition.SetScene(Self.mainMenuObject)
			Vsat.StartFadeIn(transition)
			
			backgroundColor.Set(mainMenuObject.backgroundColor)
			Local fadeColor:= New VFadeToColorAction(backgroundColor, normalBgColor, 0.5, LINEAR_TWEEN)
			AddAction(fadeColor)
			
			FadeInPlayerAnimation(1.5)
			isFirstTime = True
		Else
			transitionInDone = True
		End
	End
	
	Method FadeInPlayerAnimation:Void(delayTime:Float)
		transitionInDone = False
		
		player.color.Alpha = 0.0
		player.SetScale(0.1)
		player.isIntroAnimating = True
		
		Local groupTime:Float = 0.5
		If Vsat.IsChangingScenes() Then groupTime = 0.8
		Local delay:= New VDelayAction(delayTime)
		Local alpha:= New VFadeToAlphaAction(player.color, 1.0, groupTime, LINEAR_TWEEN)
		Local scale:= New VVec2ToAction(player.scale, 1.0, 1.0, groupTime, EASE_OUT_BACK)
		Local group:= New VActionGroup([VAction(alpha), VAction(scale)])
		Local sequence:= New VActionSequence([VAction(delay), VAction(group)])
		sequence.Name = "HeroIntroAnimation"
		AddAction(sequence)
	End
	
	Method InitEffects:Void()
		Local effect:Object = Vsat.RestoreFromClipboard("BgEffect")
		If effect
			backgroundEffect = ParticleBackground(effect)
		End
		
		Local baseUnit:Float = Vsat.ScreenWidth * 0.5
		explosionEffect = New ExplosionEmitter
		explosionEffect.InitWithSize(60)
		explosionEffect.particleLifeSpan = 0.8
		explosionEffect.oneShot = True
		explosionEffect.speed = baseUnit * 4
		explosionEffect.slowDownSpeed = 0.85
		explosionEffect.size.Set(baseUnit/20, baseUnit/20)
		explosionEffect.endSize.Set(baseUnit/500, baseUnit/500)
		explosionEffect.endColor.Alpha = 0.0
		explosionEffect.emissionAngleVariance = 180
	End
	
	Method InitFeeds:Void()
		medalFeed = New LabelFeed
		medalFeed.InitWithSizeAndFont(5, RealPath("font"))
		medalFeed.SetIcon(RealPath("medal.png"))
		medalFeed.position.Set(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.65)
		medalFeed.SetCallback(Self)
		
		scoreFeed = New LabelFeed
		scoreFeed.InitWithSizeAndFont(5, RealPath("font"))
		scoreFeed.SetAlignment(AngelFont.ALIGN_LEFT, AngelFont.ALIGN_CENTER)
		scoreFeed.sampleText = "+5"
		scoreFeed.position.Set(Vsat.ScreenWidth * 0.8, Vsat.ScreenHeight * 0.65 + 8)
		scoreFeed.lineHeightMultiplier = 0.7
	End
	
	Method InitAds:Void()
		#If TARGET = "ios"
			If IsUnlocked = False And admob = Null
				admob = Admob.GetAdmob()	
			End
		#End
		ShowAds()
	End
	
	
'--------------------------------------------------------------------------
' * Helpers
'--------------------------------------------------------------------------
	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	Method HasSurprise:Bool()
		Return Rnd() < 0.1 And dodged >= 5 And lastSurpriseRound > 2
	End
	
	Method UsedActionKey:Bool()
		Return KeyHit(KEY_SPACE) Or MouseHit()
	End
	
	Method RandomTip:String[]()
		If Not isFirstTime Return [""]
		
		If Medals.NormalDodge < 3
			Return ["Tap to jump.", "Dodge the blocks."]
		End
		
		Local tipArray:String[] = New String[3]
		tipArray[0] = "You can tap jump~neven before you hit a wall."
		tipArray[1] = "Got Headphones?~nYou can hear from which side~nthe blocks will come from."
		tipArray[2] = "In the Medals screen click on a medal~nto get more info."
		
		Local index:Int = Int(Rnd(tipArray.Length))
		Local returnTip:String[] = tipArray[index].Split("~n")
		Return returnTip
	End
	
	Method HideAds:Void()
		#If TARGET = "ios"
			If IsUnlocked = False And admob
				admob.HideAdView()
			End
		#End
	End
	
	Method ShowAds:Void()
		#If TARGET = "ios"
			If IsUnlocked = False And admob
				admob.ShowAdView(adStyle, adLayout)
			End
		#End
	End


'--------------------------------------------------------------------------
' * HyperMode
'--------------------------------------------------------------------------
	Method ActivateHyperMode:Void()
		hyperModeIsActive = True
		Enemy.COLLISION_FORGIVENESS = 0.2
		hyperModeTimer = HYPER_TIME
	End
	
	Method DeactivateHyperMode:Void()
		hyperModeIsActive = False
		Enemy.COLLISION_FORGIVENESS = 0.1
	End
	
	Method HyperModeActive:Bool() Property
		Return hyperModeIsActive
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		UpdateActions(dt)
		UpdateCursor()
		UpdateBackgroundEffect(dt)
		explosionEffect.Update(dt)
		
		If gameOver
			UpdateWhileGameOver(dt)
			Return
		End
		
		If transitionInDone = False
			Return
		End
		
		If UsedActionKey() Then player.Jump()
		
		player.Update(dt)
		UpdateEnemySpawning(dt)
		UpdateEnemies(dt)
		CheckForDodged()
		UpdateCollision()
		UpdateHyperMode(dt)
		UpdateScore()
		UpdateMedals(dt)
	End
	
	Method UpdateActions:Void(dt:Float)
		For Local action:= EachIn actions
			action.Update(dt)
		Next
	End
	
	Method UpdateCursor:Void()
		If TouchDown()
			lastTouchDown = True
			OnMouseDown()
		Else
			If lastTouchDown
				OnMouseUp()
			End
			lastTouchDown = False
		End
	End
	
	Method UpdateBackgroundEffect:Void(dt:Float)
		If backgroundEffect
			If gameOver
				backgroundEffect.Update(dt*0.3)
			Else
				backgroundEffect.Update(dt)
			End
		End
	End
	
	Method UpdateEnemies:Void(dt:Float)
		For Local e:= EachIn enemies
			e.Update(dt)
		Next
		doubleBall.Update(dt)
	End

	Method UpdateEnemySpawning:Void(dt:Float)
		If waitForSpurprise Return
			
		enemyTimer -= dt
		If enemyTimer <= 0.0
			lastSurpriseRound += 1
			If hyperModeIsActive
				enemyTimer = 0.7 + Rnd() * SPAWN_TIME_RANGE/1.8
			Else
				enemyTimer = 0.7 + Rnd() * SPAWN_TIME_RANGE
			End
			
			If HasSurprise()
				FlashScreenBeforeSurprise()
				If enemyTimer < 1.2 Then enemyTimer = 1.2
			Else
				SpawnEnemy()
			End
		End
	End
	
	Method UpdateCollision:Void()
		For Local enemy:= EachIn enemies
			If enemy.hasBeenScored = False And enemy.CollidesWith(player)
				OnCollisionWithEnemy(enemy)
			End
		Next
		If doubleBall.HasCollided() = False And doubleBall.CollidesWith(player)
			OnDoubleBallCollision()
		End
	End
	
	Method CheckForDodged:Void()
		For Local e:= EachIn enemies
			If Not e.hasBeenScored And e.position.y > player.position.y + player.size.y
				dodged += 1
				e.hasBeenScored = True
			End
		Next
	End
	
	Method UpdateScore:Void()
		If score < targetScore
			If targetScore - score <= 5
				If Vsat.Frame Mod 2 = 0
					score += 1
				End
			Else
				score += 1
			End
		End
	End
	
	Method UpdateMedals:Void(dt:Float)
		If medalFeed.IsFull
			Medals.EmitEvent("Feeder")
		End
		
		Medals.Update(Self)
		medalFeed.Update(dt)
		scoreFeed.Update(dt)
	End
	
	Method UpdateHyperMode:Void(dt:Float)
		If hyperModeIsActive
			hyperModeTimer -= dt
			If hyperModeTimer <= 0
				hyperModeTimer = 0.0
				DeactivateHyperMode()
			End
		End
	End
	
	Method UpdateWhileGameOver:Void(dt:Float)
		gameOverState.Update(dt)
		If backButton.color.Alpha < 1.0
			backButton.color.Alpha += dt * 4
		End
	End
	
	Method OnUpdateWhilePaused:Void()
		UpdateActions(0.016666)
	End
	
	
'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------	
	Method OnRender:Void()
		If backgroundEffect Then backgroundEffect.Render()
		RenderTip()
		
		If Vsat.IsChangingScenes()
			If MoveUpTransition(Vsat.transition)
				Local transition:= MoveUpTransition(Vsat.transition)
				If transition.startPoint > 0
					Translate(0, -Vsat.ScreenHeight * Vsat.transition.Progress * 0.2)
				End
			End
		End
		
		player.Render()
		RenderEnemies()
		explosionEffect.Render()
		RenderScore()
		medalFeed.Render()
		scoreFeed.Render()
		RenderHyperMode()
		
		RenderScreenFlash()
		
		RenderGameOver()
		backButton.Render()
	End
	
	Method RenderScore:Void()
		ResetColor()
		Color.White.UseWithoutAlpha()
		If Vsat.transition
			SetAlpha(Min(Vsat.transition.Progress*2, 1.0))
		Else
			SetAlpha(1.0)
		End
		
		Local scoreRatio:Float = 1.5 - Float(score)/targetScore * 0.5
		PushMatrix()
			If score > 0 And targetScore > 0
				ScaleAt(Vsat.ScreenWidth2, backButton.position.y+8, scoreRatio, scoreRatio)
			End
			scoreFont.DrawText(score, Vsat.ScreenWidth2, backButton.position.y+8, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
		PopMatrix()
	End
	
	Method RenderEnemies:Void()
		For Local e:= EachIn enemies
			e.Render()
		Next
		doubleBall.Render()
	End
	
	Method RenderGameOver:Void()
		If gameOver
			gameOverState.Render()
		End
	End
	
	Method RenderScreenFlash:Void()
		ResetBlend()
		surpriseColor.Use()
		DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
	End
	
	Method RenderTip:Void()
		If isFirstTime
			ResetBlend()
			If Vsat.transition
				SetAlpha(Min(Vsat.transition.Progress * 3.0, 1.0))
			Else
				Local normalTipLength:Float = 15
				randomTipAlpha -= Vsat.DeltaTime * randomTip.Length/normalTipLength * 3
				If randomTipAlpha <= 0
					randomTipAlpha = 0.0
					isFirstTime = False
				End
				SetAlpha(randomTipAlpha)
			End
			
			Color.Yellow.UseWithoutAlpha()
			PushMatrix()
				Translate(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.3 - scoreFont.height * 0.9)
				Scale(0.8, 0.8)
				scoreFont.DrawText(tip, 0, 0, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
			PopMatrix()
			
			Color.White.UseWithoutAlpha()
			For Local i:Int = 0 Until randomTip.Length
				PushMatrix()
					Translate(Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.3 + i*scoreFont.height*0.8)
					Scale(0.8, 0.8)
					scoreFont.DrawText(randomTip[i], 0, 0, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
				PopMatrix()
			Next
		End
	End
	
	Method RenderHyperMode:Void()
		If hyperModeIsActive
			ResetBlend()
			Color.Yellow.Use()
			SetAlpha(0.6)
			Local x:Float = Vsat.ScreenWidth2 + scoreFont.TextWidth(Highscore) * 1.5 + Vsat.ScreenWidth*0.05
			scoreFont.DrawText("x 2", x, backButton.position.y+8, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
			
			Local x2:Float = x + TextWidth("x 2") * 1.5
			Local w:Float = Vsat.ScreenWidth2 * 0.3
			Local h:Float = 8
			Local y:Float = backButton.position.y + scoreFont.TextHeight("0")/3 - h/2
			If IsHD()
				h *= 2
			End
			SetAlpha(0.15)
			DrawRect(x2, y, w, h)
			SetAlpha(0.6)
			w = w - (w * (1 - hyperModeTimer/HYPER_TIME))
			DrawRect(x2, y, w, h)
		End
	End
	
	Method ClearScreen:Void()
		ClearScreenWithColor(backgroundColor)
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method HandleEvent:Void(event:VEvent)
		If event.id.StartsWith("medal_")
			GotMedal(event.id[6..], event.x)
			Return
		End
		
		Select event.id
			Case "GameOver"
				OnGameOver()
			Case "RemoveEnemy"
				' score += 1
			Case "ReturnToGame"
				ReturnFromGameOver()
		End
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			Select action.Name
				Case "Double"
					Vsat.paused = False
				Case "SurpriseFlash"
					Surprise()
				Case "TransitionInDone"
					transitionInDone = True
				Case "HeroIntroAnimation"
					player.isIntroAnimating = False
					If action.Duration < 1.5
						transitionInDone = True
					End
				Case "GameOverDelay"
					gameOverState.Activate()
			End
		End
	End
	
	Method OnLabelPush:Void(item:LabelFeedItem)
		If Not feedSound
			feedSound = Audio.GetSound("audio/feed.mp3")
			Assert(feedSound)
		End
		Local channel:Int = 10 + medalFeed.ActiveItems
		Audio.PlaySound(feedSound, channel)
	End
	
	Method NewHighscore:Void()
		SyncGameCenter(Highscore)
	End
	
	Method Surprise:Void()
		waitForSpurprise = False
		lastSurpriseRound = 0
		Local rand:Float = Rnd()
		
		Local probability1:Float = 0.4
		Local probability2:Float = 0.8
		If hyperModeIsActive
			probability1 = 0.5
			probability2 = 1.0
		End
		rand = 0.9
		
		If rand < probability1
			Local e:= New Enemy
			e.SetLeft()
			e.link = enemies.AddLast(e)
			e.isSurprise = True 'only 1 is set as surprise to prevent double counting medals
			Local e2:= New Enemy
			e2.SetRight()
			e2.link = enemies.AddLast(e2)
		ElseIf rand <= probability2
			Local e:= New Enemy
			e.isSurprise = True
			e.SetCenter()
			e.link = enemies.AddLast(e)
		Else
			If hyperModeIsActive = False
				doubleBall.Start()
			End
		End
	End
	
	Method SpawnEnemy:Void()
		Local e:= New Enemy
		If Rnd() > 0.5
			e.SetLeft()
		Else
			e.SetRight()
		End
		e.link = enemies.AddLast(e)
	End
	
	Method OnCollisionWithEnemy:Void(enemy:Enemy)
		enemy.collidedWithPlayer = True
		Medals.EmitEvent("Direct Hit")
		OnGameOver()
	End
	
	Method OnDoubleBallCollision:Void()
		ActivateHyperMode()
		Local action:= New VVec2ToAction(doubleBall.scale, 0.0, 0.0, 0.8, EASE_OUT_QUAD)
		Local delay:= New VDelayAction(0.05)
		delay.Name = "Double"
		AddAction(delay)
		AddAction(action)
		Audio.PlaySound(Audio.GetSound("audio/double.mp3"), 21)
		Vsat.paused = True
		Medals.EmitEvent("Highflyer")
	End
	
	Method ResetGame:Void()
		score = 0
		targetScore = 0
		dodged = 0
		gameOver = False
		
		player.Reset()
		
		enemies.Clear()
		enemyTimer = 1.4
		
		surpriseColor.Set(Color.White)
		surpriseColor.Alpha = 0.0
		waitForSpurprise = False
		
		isFirstTime = False
		
		Medals.ResetThisRound()
	End
	
	Method FlashScreenBeforeSurprise:Void()
		Local fadeIn:= New VFadeToAlphaAction(surpriseColor, 0.8, 0.2, EASE_IN_QUAD)
		Local fadeOut:= New VFadeToAlphaAction(surpriseColor, 0.0, 0.1, EASE_OUT_QUAD)
		Local sequence:= New VActionSequence
		sequence.Name = "SurpriseFlash"
		sequence.AddAction(fadeIn)
		sequence.AddAction(fadeOut)
		AddAction(sequence)
		
		Audio.PlaySound(surpriseSound, 30)
	End
	
	Method OnGameOver:Void()
		Local delay:= New VDelayAction(0.3)
		delay.Name = "GameOverDelay"
		AddAction(delay)
		
		gameOver = True
		scoreMannedThisRound = False
		
		DeactivateHyperMode()
		hyperModeTimer = 0.0
		doubleBall.Reset()
		
		score = targetScore
		If score > Highscore
			Highscore = score
			NewHighscore()
		End
		
		medalFeed.Clear()
		scoreFeed.Clear()
		
		player.color.Alpha = 0.0
		enemies.Clear()
		
		Local fadeColor:= New VFadeToColorAction(backgroundColor, gameOverColor, 0.2, LINEAR_TWEEN)
		AddAction(fadeColor)
		
		explosionEffect.position.Set(player.position)
		explosionEffect.Start()
		Audio.StopAllSounds()
		Audio.PlaySound(Audio.GetSound("audio/explosion.mp3"), 5)
		
		backButton.color.Alpha = 0.0
		
		Medals.UpdatePostGame(Self)
		SaveGame()
	End
	
	Method GotMedal:Void(id:String, andPoints:Int)
		If (id = "Scoreman")
			If scoreMannedThisRound Return
			scoreMannedThisRound = True
		End
		
		If andPoints = 0
			Return
		End
		If hyperModeIsActive Then andPoints *= 2
			
		medalFeed.Push(id)
		scoreFeed.Push("+" + andPoints)
		targetScore += andPoints
	End
	
	Method OnMouseUp:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If backButton.isDown
			backButton.isDown = False
			If backButton.WasTouched(cursor)
				OnBackClicked()
			End
			Return
		ElseIf gameOver
			gameOverState.OnMouseUp()
		End
	End
	
	Method OnMouseDown:Void()
		Local cursor:Vec2 = New Vec2(TouchX(), TouchY())
		If backButton.WasTouched(cursor)
			backButton.isDown = True
		ElseIf gameOver
			gameOverState.OnMouseDown()
		End
	End
	
	Method OnBackClicked:Void()
		If gameOver
			If Vsat.transition Return
			AddAction(New VFadeToAlphaAction(globalAlpha, 0.0, 0.5, EASE_OUT_EXPO))
			mainMenuObject.shouldClearScreen = True
			Local transition:= New MoveUpTransition(0.7)
			transition.startPoint = Vsat.ScreenHeight
			transition.SetScene(mainMenuObject)
			Vsat.ChangeSceneWithTransition(mainMenuObject, transition)

			Local fadeColor:= New VFadeToColorAction(backgroundColor, mainMenuObject.backgroundColor, 0.5, LINEAR_TWEEN)
			AddAction(fadeColor)
			
			HideAds()
			backgroundEffect.SetNormal()
			Local sound:= Audio.GetSound("audio/fadeout.mp3")
			Audio.PlaySound(sound, 2)
		End
	End
	
	Method ReturnFromGameOver:Void()
		FadeInPlayerAnimation(0.25)
		ResetGame()
		Local fadeColor:= New VFadeToColorAction(backgroundColor, normalBgColor, 0.3, LINEAR_TWEEN)
		AddAction(fadeColor)
		Local fadeBack:= New VFadeToAlphaAction(backButton.color, 0.0, 0.2, LINEAR_TWEEN)
		AddAction(fadeBack)
	End
	

'--------------------------------------------------------------------------
' * Private
'--------------------------------------------------------------------------
	Private
	Field randomTip:String[]
	Field tip:String
	Field randomTipAlpha:Float = 1.0
	
	Field scoreFont:AngelFont
	Field actions:List<VAction> = New List<VAction>
	
	Field mainMenuObject:MainMenu
	Field backgroundEffect:ParticleBackground
	Field explosionEffect:ExplosionEmitter
	
	Field medalFeed:LabelFeed
	Field scoreFeed:LabelFeed
	
	Field feedSound:Sound
	
	Field hyperModeIsActive:Bool
	
	'admob
	#If TARGET = "ios"
		Field admob:Admob
		Field adLayout:Int = 5
		Field adStyle:Int = 2
	#End
	
End









