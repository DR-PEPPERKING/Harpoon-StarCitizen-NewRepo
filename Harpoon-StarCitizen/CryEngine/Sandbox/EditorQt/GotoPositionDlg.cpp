// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "GotoPositionDlg.h"

#include "ViewManager.h"
#include "GameEngine.h"
#include "Commands/CommandManager.h"

#include <RenderViewport.h>

#include <QFormLayout>
#include <QVBoxLayout>
#include <QSpacerItem>

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>

class CPositionSpinBox : public QDoubleSpinBox
{
public:
	CPositionSpinBox(CGotoPositionDialog* pParent)
		: QDoubleSpinBox(pParent)
	{
		setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
		setRange(-64000.0, 64000.0);
		setSingleStep(0.01);
		setValue(0.0);
	}
};

class CRotationSpinBox : public QDoubleSpinBox
{
public:
	CRotationSpinBox(CGotoPositionDialog* pParent)
		: QDoubleSpinBox(pParent)
	{
		setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
		setRange(-360.0, 360.0);
		setSingleStep(0.1);
		setValue(0.0);
	}
};

class CSegmentSpinBox : public QSpinBox
{
public:
	CSegmentSpinBox(CGotoPositionDialog* pParent, int32 rangeMin, int32 rangeMax, int32 initValue)
		: QSpinBox(pParent)
	{
		setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
		setRange(rangeMin, rangeMax);
		setSingleStep(1);
		setValue(initValue);
	}
};

CGotoPositionDialog::CGotoPositionDialog()
	: CEditorDialog("Go to position")
{
	setWindowTitle(tr("Go to position"));

	QVBoxLayout* pDialogLayout = new QVBoxLayout(this);

	QFormLayout* pFormLayout = new QFormLayout();
	pDialogLayout->addLayout(pFormLayout);

	QLabel* pDescLine = new QLabel(tr("Enter position here:"));
	pFormLayout->addRow(pDescLine);

	Vec3 position(0.0f);
	Vec3 rotation(0.0f);
	CViewport* pViewport = GetIEditorImpl()->GetViewManager()->GetSelectedViewport();
	if (pViewport && pViewport->IsRenderViewport() && pViewport->GetType() == ET_ViewportCamera)
	{
		CRenderViewport* pRenderViewport = static_cast<CRenderViewport*>(pViewport);
		Matrix34 transform = pRenderViewport->GetViewTM();
		position = transform.GetTranslation();
		rotation = Vec3(RAD2DEG(Ang3::GetAnglesXYZ(transform)));
	}

	m_pPositionString = new QLineEdit(this);
	m_pPositionString->setText(QString("%1, %2, %3, %4, %5, %6").arg(position.x).arg(position.y).arg(position.z).arg(rotation.x).arg(rotation.y).arg(rotation.z));
	pFormLayout->addRow(m_pPositionString);
	m_pPositionString->setFocus();
	m_pPositionString->selectAll();

	connect(m_pPositionString, &QLineEdit::editingFinished, this, &CGotoPositionDialog::OnPositionStringChanged);
	QGridLayout* pGridLayout = new QGridLayout(this);
	pDialogLayout->addLayout(pGridLayout);

	QLabel* pPosColumnHeader = new QLabel(tr("Position"));
	QLabel* pRotColumnHeader = new QLabel(tr("Rotation"));

	pGridLayout->addWidget(pPosColumnHeader, 0, 0);
	pGridLayout->addWidget(pRotColumnHeader, 0, 1);

	m_pPositionBoxes[0] = new CPositionSpinBox(this);
	m_pPositionBoxes[1] = new CPositionSpinBox(this);
	m_pPositionBoxes[2] = new CPositionSpinBox(this);

	m_pPositionBoxes[0]->setValue(position.x);
	m_pPositionBoxes[1]->setValue(position.y);
	m_pPositionBoxes[2]->setValue(position.z);

	m_pRotationBoxes[0] = new CRotationSpinBox(this);
	m_pRotationBoxes[1] = new CRotationSpinBox(this);
	m_pRotationBoxes[2] = new CRotationSpinBox(this);

	m_pRotationBoxes[0]->setValue(rotation.x);
	m_pRotationBoxes[1]->setValue(rotation.y);
	m_pRotationBoxes[2]->setValue(rotation.z);

	pGridLayout->addWidget(m_pPositionBoxes[0], 1, 0);
	pGridLayout->addWidget(m_pPositionBoxes[1], 2, 0);
	pGridLayout->addWidget(m_pPositionBoxes[2], 3, 0);
	pGridLayout->addWidget(m_pRotationBoxes[0], 1, 1);
	pGridLayout->addWidget(m_pRotationBoxes[1], 2, 1);
	pGridLayout->addWidget(m_pRotationBoxes[2], 3, 1);

	connect(m_pPositionBoxes[0], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);
	connect(m_pPositionBoxes[1], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);
	connect(m_pPositionBoxes[2], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);
	connect(m_pRotationBoxes[0], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);
	connect(m_pRotationBoxes[1], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);
	connect(m_pRotationBoxes[2], static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &CGotoPositionDialog::OnValueChanged);

	QSpacerItem* pSpacer = new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
	pDialogLayout->addSpacerItem(pSpacer);

	QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
	pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	pDialogLayout->addWidget(pButtonBox);
	connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(pButtonBox, &QDialogButtonBox::accepted, this, &CGotoPositionDialog::GotoPosition);
}

CGotoPositionDialog::~CGotoPositionDialog()
{

}

void CGotoPositionDialog::OnPositionStringChanged()
{
	if (!m_pPositionString->text().isEmpty())
	{
		const QByteArray byteArray = m_pPositionString->text().toUtf8();
		const char* pPosition = byteArray.data();
		const size_t numValues = 6; // Position XYZ and Rotation XYZ

		float value[numValues] = { 0 };
		char c = ~0;

		for (size_t i = 0; i < numValues && c != 0; ++i)
		{
			// Note: VS 2015 produces wrong code when using a for loop which but a while loop works.
			//for(c = *pPosition; c != 0 && (c < '0' || c > '9'); ++pPosition);

			// Search number begin ...
			while (c = *pPosition, c != 0 && c != '-' && (c<'0' || c> '9'))
			{
				++pPosition;
			}

			sscanf(pPosition, "%f", &value[i]);

			// ... and now for the number end.
			while (c = *pPosition++, (c >= '0' && c <= '9') || c == '.' || c == '-');
		}

		m_pPositionBoxes[0]->setValue(value[0]);
		m_pPositionBoxes[1]->setValue(value[1]);
		m_pPositionBoxes[2]->setValue(value[2]);

		m_pRotationBoxes[0]->setValue(value[3]);
		m_pRotationBoxes[1]->setValue(value[4]);
		m_pRotationBoxes[2]->setValue(value[5]);
	}
}

void CGotoPositionDialog::OnValueChanged()
{
	stack_string positionString;
	positionString.Format(
		"%.2f, %.2f, %.2f, %.2f, %.2f, %.2f",
		float(m_pPositionBoxes[0]->value()),
		float(m_pPositionBoxes[1]->value()),
		float(m_pPositionBoxes[2]->value()),
		float(m_pRotationBoxes[0]->value()),
		float(m_pRotationBoxes[1]->value()),
		float(m_pRotationBoxes[2]->value())
	);

	m_pPositionString->setText(positionString.c_str());
}

void CGotoPositionDialog::GotoPosition()
{
	stack_string command;
	command.Format(
	  "general.set_current_view_position %f %f %f",
	  float(m_pPositionBoxes[0]->value()),
	  float(m_pPositionBoxes[1]->value()),
	  float(m_pPositionBoxes[2]->value())
	  );
	GetIEditorImpl()->GetCommandManager()->Execute(command.c_str());

	command.Format(
	  "general.set_current_view_rotation %f %f %f",
	  float(m_pRotationBoxes[0]->value()),
	  float(m_pRotationBoxes[1]->value()),
	  float(m_pRotationBoxes[2]->value())
	  );
	GetIEditorImpl()->GetCommandManager()->Execute(command.c_str());

	QDialog::accept();
}

namespace Private_GoToPositionCommands
{
	void GoToPosition()
	{
		CGotoPositionDialog dlg;
		dlg.exec();
	}
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_GoToPositionCommands::GoToPosition, level, go_to_position,
                                   CCommandDescription("Opens go to position dialog"))
REGISTER_EDITOR_UI_COMMAND_DESC(level, go_to_position, "Go to Position...", "", "icons:Tools/Go_To_Position.ico", false)
REGISTER_COMMAND_REMAPPING(ui_action, actionGoto_Position, level, go_to_position)
