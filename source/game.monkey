Strict
Import vsat
Import extra
Import player
Import enemy
Import menu
Import medals


Class GameScene Extends VScene Implements VActionEventHandler

	Global Highscore:Int
		
	Field player:Player
	Field enemies:List<Enemy>
	Field enemyTimer:Float = 2.5
	Field gameOver:Bool
	Field score:Int
	
	Field surpriseChance:Int = 20
	Field surpriseCounter:Int
	Field surpriseColor:Color = New Color
	Field waitForSpurprise:Bool
	Field surpriseSound:Sound
	
	Field transitionInDone:Bool = False
	Field isFirstTime:Bool
	
	
'--------------------------------------------------------------------------
' * Init & Helpers
'--------------------------------------------------------------------------
	Method OnInit:Void()
		player = New Player
		enemies = New List<Enemy>
		
		scoreFont = New AngelFont
		scoreFont.LoadFromXml("lane_narrow")
		
		surpriseSound = LoadSound("surprise.mp3")
		
		ResetGame()
		FadeInAnimation()
		randomTip = GetRandomTip()
		
	End
	
	Method FadeInAnimation:Void()
		Local mainMenu:Object = Vsat.RestoreFromClipboard("MainMenu")
		If mainMenu
			Local delay:= New VDelayAction(2.3)
			delay.Name = "TransitionInDone"
			AddAction(delay)
			
			Local transition:= New MoveDownTransition(1.5)
			transition.SetScene(MainMenu(mainMenu))
			Vsat.StartFadeIn(transition)
			
			FadeInPlayerAnimation(1.5)
			isFirstTime = True
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
	

	Method AddAction:Void(action:VAction)
		action.AddToList(actions)
		action.SetListener(Self)
	End
	
	Method HasSurprise:Bool()
		Return Rnd() < 0.1 And score >= 8
	End
	
	Method UsedActionKey:Bool()
		Return KeyHit(KEY_SPACE) Or MouseHit()
	End
	
	Method GetRandomTip:String()
		If isFirstTime
			Return "Tap to jump"
		End
		Return ""
	End
	
	
'--------------------------------------------------------------------------
' * Update
'--------------------------------------------------------------------------
	Method OnUpdate:Void(dt:Float)
		UpdateActions(dt)
		
		If gameOver
			If UsedActionKey()
				FadeInPlayerAnimation(0.25)
				ResetGame()
			End
			Return
		End
		
		If transitionInDone = False
			Return
		End
		
		If UsedActionKey() Then player.Jump()
		
		player.UpdatePhysics(dt)
		UpdateEnemySpawning(dt)
		UpdateEnemyPhysics(dt)
		CheckForScore()
		UpdateCollision()
		
		Medals.Update(Self)
	End
	
	Method UpdateActions:Void(dt:Float)
		For Local action:= EachIn actions
			action.Update(dt)
		Next
	End
	
	Method UpdateEnemyPhysics:Void(dt:Float)
		For Local e:= EachIn enemies
			e.UpdatePhysics(dt)
		Next
	End

	Method UpdateEnemySpawning:Void(dt:Float)
		If waitForSpurprise Return
			
		enemyTimer -= dt
		If enemyTimer <= 0.0
			enemyTimer = 0.6 + Rnd() * 2
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
				OnGameOver()
			End
		Next
	End

	Method CheckForScore:Void()
		For Local e:= EachIn enemies
			If Not e.hasBeenScored And e.position.y > player.position.y + player.size.y
				score += 1
				e.hasBeenScored = True
			End
		Next
	End
	
	
'--------------------------------------------------------------------------
' * Render
'--------------------------------------------------------------------------	
	Method OnRender:Void()
		RenderTip()
		
		player.Render()
		RenderEnemies()
		RenderScore()
		RenderScreenFlash()
		
		RenderGameOver()
	End
	
	Method RenderScore:Void()
		ResetColor()
		Color.NewBlack.Use()
		If Vsat.transition
			SetAlpha(Min(Vsat.transition.Progress*2, 0.5))
		Else
			SetAlpha(0.5)
		End
		scoreFont.DrawText(score, Vsat.ScreenWidth2, 16, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_TOP)
	End
	
	Method RenderEnemies:Void()
		For Local e:= EachIn enemies
			e.Render()
		Next
	End
	
	Method RenderGameOver:Void()
		If gameOver
			Color.NewRed.Use()
			DrawRect(0, 0, Vsat.ScreenWidth, Vsat.ScreenHeight)
			Color.White.Use()
			If score = 1
				scoreFont.DrawText("You dodged 1 object", Vsat.ScreenWidth2, Vsat.ScreenHeight2, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
			Else
				scoreFont.DrawText("You dodged " + score + " objects", Vsat.ScreenWidth2, Vsat.ScreenHeight2, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
			End
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
			Color.Gray.Use()
			If Vsat.transition
				SetAlpha(Vsat.transition.Progress * 3)
			Else
				randomTipAlpha -= Vsat.DeltaTime * 2
				If randomTipAlpha <= 0
					randomTipAlpha = 0.0
					isFirstTime = False
				End
				SetAlpha(randomTipAlpha)
			End
			scoreFont.DrawText(randomTip, Vsat.ScreenWidth2, Vsat.ScreenHeight * 0.3, AngelFont.ALIGN_CENTER, AngelFont.ALIGN_CENTER)
		End
	End
	
	Method ClearScreen:Void()
		ClearScreenWithColor(Color.Silver)
	End
	
	
'--------------------------------------------------------------------------
' * Events
'--------------------------------------------------------------------------
	Method HandleEvent:Void(event:VEvent)
		If event.id.StartsWith("medal_")
			GotMedal(event.id[6..])
			Return
		End
		
		Select event.id
			Case "GameOver"
				OnGameOver()
			Case "RemoveEnemy"
				' score += 1
		End
	End
	
	Method OnActionEvent:Void(id:Int, action:VAction)
		If id = VAction.FINISHED
			Select action.Name
				Case "SurpriseFlash"
					Surprise()
				Case "TransitionInDone"
					transitionInDone = True
				Case "HeroIntroAnimation"
					player.isIntroAnimating = False
					If action.Duration < 1.5
						transitionInDone = True
					End
					
			End
		End
	End
	
	Method NewHighscore:Void()
		
	End
	
	Method Surprise:Void()
		waitForSpurprise = False
		Local rand:Float = Rnd()
		If rand < 0.5
			Local e:= New Enemy
			e.SetLeft()
			e.link = enemies.AddLast(e)
			e.isSurprise = True 'only 1 is set as surprise to prevent double counting medals
			Local e2:= New Enemy
			e2.SetRight()
			e2.link = enemies.AddLast(e2)
		Else
			Local e:= New Enemy
			e.isSurprise = True
			e.position.x = Vsat.ScreenWidth2 - e.size.x/2
			e.gravity = 30
			e.link = enemies.AddLast(e)
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
	
	Method ResetGame:Void()
		score = 0
		gameOver = False
		
		player.Reset()
		
		enemies.Clear()
		enemyTimer = 2.5
		
		surpriseColor.Set(Color.White)
		surpriseColor.Alpha = 0.0
		waitForSpurprise = False
		
		isFirstTime = False
	End
	
	Method FlashScreenBeforeSurprise:Void()
		Local fadeIn:= New VFadeToAlphaAction(surpriseColor, 0.8, 0.2, EASE_IN_QUAD)
		Local fadeOut:= New VFadeToAlphaAction(surpriseColor, 0.0, 0.1, EASE_OUT_QUAD)
		Local sequence:= New VActionSequence
		sequence.Name = "SurpriseFlash"
		sequence.AddAction(fadeIn)
		sequence.AddAction(fadeOut)
		AddAction(sequence)
		
		VPlaySound(surpriseSound)
	End
	
	Method OnGameOver:Void()
		gameOver = True
		If score > Highscore
			Highscore = score
			NewHighscore()
		End
		Medals.UpdatePostGame(Self)
	End
	
	Method GotMedal:Void(id:String)
		Print "Medal: " + id
	End
	
	Private
	Field randomTip:String
	Field randomTipAlpha:Float = 1.0
	Field scoreFont:AngelFont
	Field actions:List<VAction> = New List<VAction>
	
End









